/*
 * Copyright 2024-2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MPP_EXAMPLES_MODELS_DATABASE_UTILS_H_
#define MPP_EXAMPLES_MODELS_DATABASE_UTILS_H_

/* mpp includes */
#include "mpp_config.h"
#include "fsl_debug_console.h"

/* database includes */
#include APP_DATABASE_INFOS

void init_database(face_t * db);

void set_new_face_embeddings(const float *person_embeddings);

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
  * @brief Adds element to the database
  *
  * This function is used to add an element corresponding to the name parameter to the database
  * without using the shell console
      * @param name The name of the person to be added to the database
      * @retval 1 Successfully added the element.
 */
 int database_add(char* name);

/*!
  * @brief Removes element from database
  *
  * This function is used to delete the element corresponding to the name parameter from the database
  * without using the shell console
      * @param name The name of the person to be removed from the database
      * @retval 1 Successfully removed the element.
 */
 int database_delete(char* name);

 /*!
  * @brief Removes all elements from database
  *
  * This function is used to remove all registered faces from the database
 */
 void database_delete_all(void);

#endif /* MPP_EXAMPLES_MODELS_DATABASE_UTILS_H_ */
