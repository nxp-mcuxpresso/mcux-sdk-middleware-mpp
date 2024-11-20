/*
 * Copyright 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Utility functions for database control using shell console
 */


#include <models/shell_database_utils.h>
#include "semphr.h"

#include "fsl_debug_console.h"

/* include model infos */
#include APP_TFLITE_MOBILEFACENET_INFO
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static int delete_person (char* Name, int size);
static int calculate_size(person * people);
static int add_person (char* Name, int position);

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/
static person *embeddings_db;
static float person_embeddings[SIZE_EMBEDDING];

/*******************************************************************************
 * Code
 ******************************************************************************/

/*
 * Get pointer to persons database.
 */
void init_database(person * db)
{
	embeddings_db = db;
}

/*
 * Get new persons embeddings.
 */
void get_new_person_embeddings(const float *new_person_embeddings)
{
	//person_embeddings = (const float *)new_person_embeddings;
	memcpy(person_embeddings, new_person_embeddings, sizeof(person_embeddings));
}

/*
 * This function is used to delete a person from the database.
 * @param Name  Name of the Person to be deleted from the database.
 * @param size size of the database.
 * @retval size of the databse.
 */
static int delete_person (char* Name, int size)
{
	int i = 0;
	int j = 0;
	int position = 0;

	for(i = 0; i < size ;i++)
	{
		if(strcmp(embeddings_db[i].name, Name) == 0)
		{
			position = i;
			PRINTF("person found at %d \r\n", position);
			for(j = position; j < size-1;j++)
			{
				strcpy(embeddings_db[j].name , embeddings_db[j+1].name);
				for (int z = 0; z <SIZE_EMBEDDING ; z++)
				{
					embeddings_db[j].embedding[z] = embeddings_db[j+1].embedding[z];
				}
			}

			/* clear last name */
            strcpy(embeddings_db[size].name , "\0");

            break;
		}
	}

	return 0;
}

/*
 * This function is used to calculate the total number of elements present in the database
 * @param struct Person The database pointer.
 * @retval size of the database.
 */
static int calculate_size(person * people)
{
	int num_persons = 0;

	while (people[num_persons].name[0] != '\0'){
		num_persons++;
	}

	return num_persons;
}

/*
 * This function is used to add a person to the database
 * @param Name  Name of the Person to be added to the database.
 * @param position Position where to add the person in th database
 * @retval size of the databse.
 */
static int add_person (char* Name, int position)
{
	strcpy(embeddings_db[position-1].name, Name);
	PRINTF("position:%d\r\n",position);

    for (int i = 0; i < SIZE_EMBEDDING ; i++) {
        embeddings_db[position-1].embedding[i] = person_embeddings[i];
    }

    return 0;
}

/*
 * Add new person to database.
 */
shell_status_t database_add(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    if (argc > 0)
    {
        if (argv[1] != NULL)
        {
            PRINTF("argument");
        }
    }
    else
    {
        PRINTF("no argument");
        return kStatus_SHELL_Error;
    }

	char  *Name = argv[1];
	SHELL_Printf("You have entered: %s  \r\n", Name);

	const int new_database_size = calculate_size(embeddings_db) + 1;
	PRINTF("size %d\r\n", new_database_size);
	add_person(Name,new_database_size);
	PRINTF("Person added name %s\r\n", embeddings_db[new_database_size-1].name);

	return kStatus_SHELL_Success;
}

/*
 * Remove person from database.
 */
shell_status_t database_delete(shell_handle_t shellHandle, int32_t argc, char **argv)
{
	char  *Name = argv[1];

	SHELL_Printf("You have entered: %s  \r\n", Name);
	int new_database_size= calculate_size(embeddings_db);
	delete_person(Name,new_database_size);
	new_database_size--;
	PRINTF("%s deleted from database. \r\n", Name);

	return kStatus_SHELL_Success;
}

/*
 * Show database.
 */
shell_status_t database_show(shell_handle_t shellHandle, int32_t argc)
{
	int new_database_size= calculate_size(embeddings_db);

	SHELL_Printf("Displaying database \r\n");
	SHELL_Printf("\r\n");

	for (int j=0 ;j < new_database_size; j++)
	{
		PRINTF("Name[%d]: %s\r\n", j, embeddings_db[j].name);
	}

	return kStatus_SHELL_Success;
}
