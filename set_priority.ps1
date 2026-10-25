# Creates a "Priority" single-select field on the project board and fills it
# from each item's priority:* label. Safe to re-run.
# Usage: powershell -ExecutionPolicy Bypass -File .\set_priority.ps1
$Owner = 'Samhitha1504-007'; $Num = 3   # project URL: github.com/users/<Owner>/projects/<Num>

# 1. Create the Priority field (ignore the error if it already exists)
gh project field-create $Num --owner $Owner --name Priority --data-type SINGLE_SELECT --single-select-options "High,Medium,Low" 2>$null

# 2. Look up the IDs we need
$projId = (gh project view $Num --owner $Owner --format json | ConvertFrom-Json).id
$field  = (gh project field-list $Num --owner $Owner --format json | ConvertFrom-Json).fields | Where-Object { $_.name -eq 'Priority' }
$opt    = @{}; $field.options | ForEach-Object { $opt[$_.name] = $_.id }

# 3. For every board item, read its priority:* label and set the field
$items = (gh project item-list $Num --owner $Owner --limit 200 --format json | ConvertFrom-Json).items
foreach ($it in $items) {
  $label = $it.labels | Where-Object { $_ -like 'priority:*' } | Select-Object -First 1
  if (-not $label) { Write-Host "NO PRIORITY LABEL: $($it.title)"; continue }
  $p = $label.Split(':')[1]
  gh project item-edit --id $it.id --project-id $projId --field-id $field.id --single-select-option-id $opt[$p] | Out-Null
  Write-Host "$($it.title) -> $p"
}
