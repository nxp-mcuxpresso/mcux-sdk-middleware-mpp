/*
 * Copyright 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MPP_EXAMPLES_MODELS_SHELL_DATABASE_UTILS_H_
#define MPP_EXAMPLES_MODELS_SHELL_DATABASE_UTILS_H_


#include "fsl_shell.h"
#include "fsl_debug_console.h"

/* mpp includes */
#include "mpp_config.h"

/* database includes */
#include APP_DATABASE_INFOS

#define SHELL_Printf PRINTF

void init_database(person * db);

void get_new_person_embeddings(const float *new_person_embeddings);

/*!
 * @brief Deletes element from database
 *
 * This function is used to delete the element corresponding to the name entered in the console from the database
     * @param shellHandle The shell module handle pointer.
     * @param argc number of arguments.
     * @param argv number of arguments.
     * @retval kStatus_SHELL_Success Successfully deleted the element.
     * @retval kStatus_SHELL_Error An error occurred.
*/
 shell_status_t database_delete(shell_handle_t shellHandle, int32_t argc, char **argv);

 /*
  * @brief Adds element to the database
  *
  * This function is used to add an element corresponding to the name entered in the console to the database
      * @param shellHandle The shell module handle pointer.
      * @param argc number of arguments.
      * @param argv number of arguments.
      * @retval kStatus_SHELL_Success Successfully added the element.
      * @retval kStatus_SHELL_Error An error occurred.
 */
 shell_status_t database_add(shell_handle_t shellHandle, int32_t argc, char **argv);

 /*
  * @brief Prints the database
  *
  * This function is used to print out all the elements in the database
      * @param shellHandle The shell module handle pointer.
      * @param argc number of arguments.
      * @retval kStatus_SHELL_Success Successfully printed the database.
      * @retval kStatus_SHELL_Error An error occurred.
 */
 shell_status_t database_show(shell_handle_t shellHandle, int32_t argc);



#endif /* MPP_EXAMPLES_MODELS_SHELL_DATABASE_UTILS_H_ */
