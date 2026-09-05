/*
 * Msh - Builtins table and dispatch.
 */
#include "msh/builtin.h"

const msh_builtin_t msh_builtins[] = {
    { "cd",      msh_bi_cd      },
    { "echo",    msh_bi_echo    },
    { "exit",    msh_bi_exit    },
    { "pwd",     msh_bi_pwd     },
    { "export",  msh_bi_export  },
    { "unset",   msh_bi_unset   },
    { "alias",   msh_bi_alias   },
    { "unalias", msh_bi_unalias },
    { "history", msh_bi_history },
    { "jobs",    msh_bi_jobs    },
    { "fg",      msh_bi_fg      },
    { "bg",      msh_bi_bg      },
    { "type",    msh_bi_type    },
    { "which",   msh_bi_which   },
    { "source",  msh_bi_source  },
    { "kill",    msh_bi_kill    },
    { "umask",   msh_bi_umask   },
    { "shopt",   msh_bi_shopt   },
    { "test",    msh_bi_test    },
    { "[",       msh_bi_test    },
    { "function", msh_bi_function },
    { NULL,      NULL           }
};