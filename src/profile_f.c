/*
 * profile_f.c
 *  <FILE_DESC>
 * 	
 *  Author: Min Si
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "profile.h"

#ifdef ENABLE_PROFILE

void armci_profile_reset_counter_(){
    ARMCI_Profile_reset_counter();
}

void armci_profile_reset_timing_(){
    ARMCI_Profile_reset_timing();
}

void armci_profile_print_timing_(char *name){
    ARMCI_Profile_print_timing(name);
}

void armci_profile_print_counter_(char *name){
    ARMCI_Profile_print_counter(name);
}
#else
void armci_profile_reset_counter_(){
}

void armci_profile_reset_timing_(){
}

void armci_profile_print_timing_(char *name){
}

void armci_profile_print_counter_(char *name){
}
#endif
