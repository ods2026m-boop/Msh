/*
 * Msh - Option flags for shopt. Defined here so they have a single home.
 */
#include "msh/builtin.h"

int msh_opt_nullglob   = 0;
int msh_opt_extglob    = 0;
int msh_opt_dotglob    = 0;
int msh_opt_nocaseglob = 0;
int msh_opt_errexit    = 0;
int msh_opt_xtrace     = 0;