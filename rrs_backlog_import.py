"""
RRS Backlog Importer — Team 5, UE24CS341A

Reads rrs_backlog.csv (title, description, labels, sprint, assignee, points)
and creates one GitHub issue per row, adds each to the Project board, and
sets its Story Points + Sprint iteration. Safe to re-run: skips issues whose
title already exists. Defaults to a dry run — pass --apply to actually create
anything.

Usage:
    python rrs_backlog_import.py --repo OWNER/RRS --project PROJECT_NUMBER
    python rrs_backlog_import.py --repo OWNER/RRS --project PROJECT_NUMBER --apply
"""
import argparse, csv, json, os, shutil, subprocess, sys, tempfile

SP_FIELD = 'Story Points'
LABEL_COLOR = 'C5DEF5'


def run(args, apply, allow_fail=False):
    """Run a gh command. In preview mode, commands that change things are only printed."""
    readonly = (args[:2] == ['gh', 'api'] and '-X' not in args and 'graphql' not in args) or args[:3] == ['gh', 'issue', 'list']
    if not apply and not readonly:
        print('   would run:', ' '.join(a if ' ' not in a else repr(a) for a in args))
        return ''
    p = subprocess.run(args, capture_output=True, text=True, encoding='utf-8')
    if p.returncode != 0:
        if allow_fail:
            return None
        sys.exit(f'\nFAILED: {" ".join(args)}\n{p.stderr.strip()}')
    return p.stdout


def graphql(query, apply, **variables):
    args = ['gh', 'api', 'graphql', '-f', f'query={query}']
    for k, v in variables.items():
        args += ['-F' if isinstance(v, int) else '-f', f'{k}={v}']
    if not apply and 'mutation' in query:
        return None
    p = subprocess.run(args, capture_output=True, text=True, encoding='utf-8')
    if p.returncode != 0:
        sys.exit(f'\nGraphQL call failed:\n{p.stderr.strip()}')
    return json.loads(p.stdout)


def find_project(owner, number):
    fields = '''id title fields(first:50){ nodes{
        ... on ProjectV2Field{ id name }
        ... on ProjectV2IterationField{ id name configuration{ iterations{ id title } } } } }'''
    for kind in ('user', 'organization'):
        q = f'query($o:String!,$n:Int!){{ {kind}(login:$o){{ projectV2(number:$n){{ {fields} }} }} }}'
        args = ['gh', 'api', 'graphql', '-f', f'query={q}', '-f', f'o={owner}', '-F', f'n={number}']
        p = subprocess.run(args, capture_output=True, text=True, encoding='utf-8')
        if p.returncode == 0:
            node = (json.loads(p.stdout).get('data') or {}).get(kind) or {}
            if node.get('projectV2'):
                return node['projectV2']
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--repo', required=True, help='OWNER/REPO, e.g. samhitha/Railway_Reservation_System_SE')
    ap.add_argument('--project', type=int, required=True, help='project number (the number at the end of the project URL)')
    ap.add_argument('--owner', help='account that owns the project (default: the repo owner)')
    ap.add_argument('--sprint-field', default='Sprint', help='name of the iteration field in the project')
    ap.add_argument('--csv', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rrs_backlog.csv'))
    ap.add_argument('--apply', action='store_true', help='make the changes (default is a preview)')
    a = ap.parse_args()
    owner = a.owner or a.repo.split('/')[0]

    if not shutil.which('gh'):
        sys.exit('GitHub CLI (gh) not found. Install it (winget install --id GitHub.cli), then run: gh auth login')
    rows = [r for r in csv.DictReader(open(a.csv, encoding='utf-8-sig')) if (r.get('title') or '').strip()]
    print(f"{'APPLYING' if a.apply else 'PREVIEW (add --apply to make changes)'}: {len(rows)} items -> {a.repo}\n")

    print('1. Assignees')
    ok = {}
    for user in sorted({r['assignee'].strip() for r in rows if r.get('assignee', '').strip()}):
        ok[user] = run(['gh', 'api', f'repos/{a.repo}/collaborators/{user}', '--silent'], True, allow_fail=True) is not None
        print(f'   {user}: {"OK" if ok[user] else "not a collaborator yet - their items will be unassigned"}')

    print('\n2. Labels')
    labels = sorted({l.strip() for r in rows for l in (r.get('labels') or '').split(';') if l.strip()})
    for name in labels:
        run(['gh', 'label', 'create', name, '--color', LABEL_COLOR, '--force', '--repo', a.repo], a.apply)
    print(f'   {len(labels)} labels')

    print('\n3. Milestones')
    have = {m['title'] for m in json.loads(run(['gh', 'api', f'repos/{a.repo}/milestones?state=all&per_page=100'], True) or '[]')}
    for s in sorted({r['sprint'].strip() for r in rows if r.get('sprint', '').strip()}):
        if s in have:
            print(f'   {s}: exists')
        else:
            run(['gh', 'api', '-X', 'POST', f'repos/{a.repo}/milestones', '-f', f'title={s}'], a.apply)

    proj = sp_field = sprint_field = None
    iterations = {}
    if a.project:
        print('\n4. Project')
        p = find_project(owner, a.project)
        if not p:
            sys.exit(f'Project {a.project} not found for {owner}. Check the number and run: gh auth refresh -s project')
        proj = p['id']
        print(f'   {p["title"]}')
        for f in p['fields']['nodes']:
            if f.get('name') == SP_FIELD:
                sp_field = f['id']
            if f.get('name') == a.sprint_field and 'configuration' in f:
                sprint_field = f['id']
                iterations = {i['title']: i['id'] for i in f['configuration']['iterations']}
        if not sp_field:
            res = graphql('mutation($p:ID!){ createProjectV2Field(input:{projectId:$p, dataType:NUMBER, name:"Story Points"}){ projectV2Field{ ... on ProjectV2Field{ id } } } }', a.apply, p=proj)
            sp_field = res['data']['createProjectV2Field']['projectV2Field']['id'] if res else 'PREVIEW'
            print('   created "Story Points" field' if a.apply else '   would create "Story Points" field')
        if sprint_field:
            print(f'   "{a.sprint_field}" iterations: {", ".join(iterations) or "none"}')
            missing = sorted({r['sprint'].strip() for r in rows} - set(iterations))
            if missing:
                print(f'   WARNING: no iteration named {", ".join(missing)} - those items get no sprint on the board')
        else:
            print(f'   WARNING: no iteration field named "{a.sprint_field}" - sprints only go into milestones')
            print('   Fix: on the project board, add a field -> Iteration -> name it "Sprint" -> create Sprint 1 and Sprint 2')

    print('\n5. Issues')
    existing = {i['title']: i['url'] for i in json.loads(run(['gh', 'issue', 'list', '--repo', a.repo, '--state', 'all', '--limit', '1000', '--json', 'title,url'], True) or '[]')}
    made = skipped = 0
    for r in rows:
        title, sprint, assignee = r['title'].strip(), r.get('sprint', '').strip(), r.get('assignee', '').strip()
        url = existing.get(title)
        if url:
            skipped += 1
            print(f'   exists  {title}')
        else:
            with tempfile.NamedTemporaryFile('w', delete=False, suffix='.md', encoding='utf-8') as tf:
                tf.write(r.get('description', '')); body = tf.name
            args = ['gh', 'issue', 'create', '--repo', a.repo, '--title', title, '--body-file', body]
            for l in (r.get('labels') or '').split(';'):
                if l.strip():
                    args += ['--label', l.strip()]
            if sprint:
                args += ['--milestone', sprint]
            if assignee and ok.get(assignee):
                args += ['--assignee', assignee]
            out = run(args, a.apply)
            os.unlink(body)
            url = out.strip().splitlines()[-1] if out else None
            made += 1
            print(f"   {'created' if a.apply else 'new    '} {title}  -> {assignee or 'unassigned'}, {sprint or 'no sprint'}, {r.get('points') or 0} pts")
        if proj and url:
            node = json.loads(run(['gh', 'api', f"repos/{a.repo}/issues/{url.rstrip('/').split('/')[-1]}"], True))['node_id']
            res = graphql('mutation($p:ID!,$c:ID!){ addProjectV2ItemById(input:{projectId:$p, contentId:$c}){ item{ id } } }', a.apply, p=proj, c=node)
            item = res['data']['addProjectV2ItemById']['item']['id'] if res else None
            if item and sp_field and (r.get('points') or '').strip():
                graphql('mutation($p:ID!,$i:ID!,$f:ID!,$n:Float!){ updateProjectV2ItemFieldValue(input:{projectId:$p, itemId:$i, fieldId:$f, value:{number:$n}}){ projectV2Item{ id } } }',
                        a.apply, p=proj, i=item, f=sp_field, n=int(r['points']))
            if item and sprint_field and sprint in iterations:
                graphql('mutation($p:ID!,$i:ID!,$f:ID!,$t:String!){ updateProjectV2ItemFieldValue(input:{projectId:$p, itemId:$i, fieldId:$f, value:{iterationId:$t}}){ projectV2Item{ id } } }',
                        a.apply, p=proj, i=item, f=sprint_field, t=iterations[sprint])

    print(f"\nDone. {made} {'created' if a.apply else 'to create'}, {skipped} already existed.")
    if not a.apply:
        print('Nothing was changed. Run again with --apply to create everything.')


if __name__ == '__main__':
    main()