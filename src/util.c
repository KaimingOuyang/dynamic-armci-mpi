/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include <armci.h>
#include <armci_internals.h>
#include <debug.h>
#include <gmr.h>


/** Fatal error, print the message and abort the program with the provided
  * error code.
  */
void ARMCI_Error(const char *msg, int code) {
  fprintf(stderr, "[%d] ARMCI Error: %s\n", ARMCI_GROUP_WORLD.rank, msg);
  fflush(NULL);
  MPI_Abort(ARMCI_GROUP_WORLD.comm, code);
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Barrier = PARMCI_Barrier
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Barrier ARMCI_Barrier
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Barrier as PARMCI_Barrier
#endif
/* -- end weak symbols block -- */

/** Barrier synchronization.  Collective on the world group (not the default
  * group!).
  */
void PARMCI_Barrier(void) {
  gmr_t *cur_mreg = gmr_list;

  ARMCI_FUNC_PROFILE_TIMING_START(PARMCI_Barrier);
  ARMCI_FUNC_PROFILE_COUNTER_INC(PARMCI_Barrier, 0);

  PARMCI_AllFence();
  MPI_Barrier(ARMCI_GROUP_WORLD.comm);

  while (cur_mreg) {
    gmr_sync(cur_mreg);
    cur_mreg = cur_mreg->next;
  }

  ARMCI_FUNC_PROFILE_TIMING_END(PARMCI_Barrier);
}

/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Fence = PARMCI_Fence
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Fence ARMCI_Fence
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Fence as PARMCI_Fence
#endif
/* -- end weak symbols block -- */

/** Wait for remote completion on one-sided operations targeting process proc.
  * In MPI-2, this is a no-op since get/put/acc already guarantee remote
  * completion.
  *
  * @param[in] proc Process to target
  */
void PARMCI_Fence(int proc) {
  gmr_t *cur_mreg = gmr_list;

  while (cur_mreg) {
    gmr_flush(cur_mreg, proc, 0);
    cur_mreg = cur_mreg->next;
  }
  return;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_AllFence = PARMCI_AllFence
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_AllFence ARMCI_AllFence
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_AllFence as PARMCI_AllFence
#endif
/* -- end weak symbols block -- */

#ifdef USE_CSP_ASYNC_CONFIG
extern int armci_async_config_flag;
void armci_resched_local_async_config(MPI_Win window) {
    MPI_Info info;
    MPI_Info_create(&info);
    /* only update local async status. */
    MPI_Info_set(info, "async_config_phases", "local-update");
    MPI_Win_set_info(window, info);
    MPI_Info_free(&info);
}

void armci_update_async_config(MPI_Win window) {
    MPI_Info info;

    MPI_Info_create(&info);

    MPI_Info_set(info, (char *) "symmetric", (char *) "true");
    /* update window async config */
    if(armci_async_config_flag == 1) {
        MPI_Info_set(info, "async_config", "on");
    } else if(armci_async_config_flag == 2) {
        MPI_Info_set(info, "async_config", "off");
    } else if(armci_async_config_flag == 3) {
        MPI_Info_set(info, "async_config", "auto");
    }

    /* enable async status remote update (only valid when current window config is auto) */
    MPI_Info_set(info, "async_config_phases", "remote-exchange");

    MPI_Win_set_info(window, info);
    MPI_Info_free(&info);
}
#endif

/** Wait for remote completion on all one-sided operations.  In MPI-2, this is
  * a no-op since get/put/acc already guarantee remote completion.
  */
void PARMCI_AllFence(void) {
  gmr_t *cur_mreg = gmr_list;

  ARMCI_FUNC_PROFILE_TIMING_START(PARMCI_AllFence);
  ARMCI_FUNC_PROFILE_COUNTER_INC(PARMCI_AllFence, 0);

  while (cur_mreg) {
    gmr_flushall(cur_mreg, 0);
    gmr_sync(cur_mreg);

    cur_mreg = cur_mreg->next;
  }

  MPI_Barrier(ARMCI_GROUP_WORLD.comm);

    if (ARMCI_GROUP_WORLD.rank == 0) {
        ARMCI_DBG_PRINT_STDOUT("called %s\n", __FUNCTION__);
    }

#ifdef USE_CSP_ASYNC_CONFIG
    int idx = 0, verbose = 0;

    /* update asynchronous progress for all windows */
    cur_mreg = gmr_list;
    armci_resched_local_async_config(cur_mreg->window);

    CSP_get_verbose(&verbose);

    cur_mreg = gmr_list;
    while (cur_mreg) {
        if(idx == 1)
            CSP_set_verbose(1);

        armci_update_async_config(cur_mreg->window);
        cur_mreg = cur_mreg->next;
        idx++;
    }

    /* reset verbose */
    CSP_set_verbose(verbose);
#endif

  ARMCI_FUNC_PROFILE_TIMING_END(PARMCI_AllFence);

  return;
}

#ifdef USE_CSP_ASYNC_CONFIG
#include <casper.h>

void armci_global_dump_async_config(const char *fname) {
  gmr_t *cur_mreg = gmr_list;
  int group_me = -1;

  while (cur_mreg) {
    MPI_Comm_rank(cur_mreg->group.comm, &group_me);

    if(group_me == 0)
        CSP_win_dump_async_config(cur_mreg->window, fname);
    cur_mreg = cur_mreg->next;
  }
  return;
}
#else
void armci_global_dump_async_config(const char *fname) {/*do nothing*/}
#endif

int ARMCI_Uses_shm(void) {
  return 0;
}


void ARMCI_Set_shm_limit(unsigned long shmemlimit) {
  return;
}


int ARMCI_Uses_shm_grp(ARMCI_Group *group) {
  return 0;
}


/** Copy local data.
  *
  * @param[in]  src  Source buffer
  * @param[out] dst  Destination buffer
  * @param[in]  size Number of bytes to copy
  */
void ARMCI_Copy(const void *src, void *dst, int size) {
#ifndef COPY_WITH_SENDRECV
  memcpy(dst, src, size);
#else
  static MPI_Comm copy_comm = MPI_COMM_NULL;

  if (copy_comm == MPI_COMM_NULL)
    MPI_Comm_dup(MPI_COMM_SELF, &copy_comm);

  MPI_Sendrecv(src, size, MPI_BYTE,
      0 /* rank */, 0 /* tag */,
      dst, size, MPI_BYTE,
      0 /* rank */, 0 /* tag */,
      copy_comm, MPI_STATUS_IGNORE);
#endif
}


/** Zero out the given buffer.
  */
void ARMCII_Bzero(void *buf, armci_size_t size) {
	/* Jeff: Why not use memset? */
  armci_size_t i;
  uint8_t *buf_b = (uint8_t *)buf;

  for (i = 0; i < size; i++)
    buf_b[i] = 0;
}


static const unsigned char log2_table[256] = 
    { 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 };

/** Calculate the base 2 logarithm of a given integer.
  */
int ARMCII_Log2(unsigned int val) {
  unsigned int v16, v8;
  int lg = 0;

  if (val == 0) return -1;

  if ((v16 = val >> 16))
    lg = (v8 = v16 >> 8) ? log2_table[v8] + 24 : log2_table[v16] + 16;
  else
    lg = (v8 = val >> 8) ? log2_table[v8] + 8 : log2_table[val];

  return lg;
}


/** Retrieve the value of a boolean environment variable.
  */
int ARMCII_Getenv_bool(const char *varname, int default_value) {
  const char *var = getenv(varname);

  if (var == NULL)
    return default_value;
  
  if (var[0] == 'T' || var[0] == 't' || var[0] == '1' || var[0] == 'y' || var[0] == 'Y')
    return 1;

  else
    return 0;
}


/** Retrieve the value of a environment variable.
  */
char *ARMCII_Getenv(const char *varname) {
  return getenv(varname);
}


/** Retrieve the value of an integer environment variable.
  */
int ARMCII_Getenv_int(const char *varname, int default_value) {
  const char *var = getenv(varname);
  if (var) 
    return atoi(var);
  else
    return default_value;
}

void ARMCIX_Progress(void)
{
    gmr_progress();
}
