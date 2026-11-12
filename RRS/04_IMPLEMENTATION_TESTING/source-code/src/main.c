#include "facade.h"
#include "tui.h"

int main(void)
{
    (void)facade_init("data/audit.log");
    (void)facade_seed_demo();
    return rrs_tui_start();
}
