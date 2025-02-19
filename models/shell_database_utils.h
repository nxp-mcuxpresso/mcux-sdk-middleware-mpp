/*
 * Copyright 2024-2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MPP_EXAMPLES_MODELS_SHELL_DATABASE_UTILS_H_
#define MPP_EXAMPLES_MODELS_SHELL_DATABASE_UTILS_H_

/* Shell includes */
#include "fsl_shell.h"
#include "fsl_debug_console.h"

/* FreeRTOS kernel includes. */
#include "task.h"

/* mpp includes */
#include "mpp_config.h"

/* database includes */
#include APP_DATABASE_INFOS

#define SHELL_Printf PRINTF

/* Shell delay */
#define SHELL_TASK_DELAY vTaskDelay(1000)

void init_database(person * db);

void set_new_person_embeddings(const float *person_embeddings);

/*
 * get registration status.
 * @retval 0 face not registered.
 * @retval 1 face registered.
 * */
int registration_state();

/*
 * reset registration status once user added.
 * */
int reset_registration_state();

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
