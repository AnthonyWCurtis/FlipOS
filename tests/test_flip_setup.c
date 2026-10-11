#include "flip_setup.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    assert(system("rm -rf /tmp/flipos-setup-test") == 0);
    assert(!flip_setup_is_complete());
    assert(!flip_setup_is_in_progress());
    assert(flip_setup_begin() == 0);
    assert(flip_setup_is_in_progress());
    assert(!flip_setup_is_complete());
    assert(flip_setup_mark_complete() == 0);
    assert(flip_setup_is_complete());
    assert(!flip_setup_is_in_progress());
    puts("flip_setup tests passed");
    return 0;
}
