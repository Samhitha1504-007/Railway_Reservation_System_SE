#ifndef RRS_TUI_H
#define RRS_TUI_H

/* ncurses front end. Talks only to the facade (facade.h) and auth.h. */

#ifdef __cplusplus
extern "C" {
#endif

/* Run the interactive screens until the user quits. Returns 0 on a normal exit. */
int rrs_tui_start(void);

#ifdef __cplusplus
}
#endif

#endif /* RRS_TUI_H */
