/*
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Utility functions for database control
 */

#include <models/database_utils.h>

/* include model infos */
#include APP_TFLITE_MOBILEFACENET_INFO

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static int delete_person (char* Name, int size);
static int calculate_size(face_t * face);
static void add_person (char* Name, int position);

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/
static face_t *embeddings_db;
static const uint32_t embeddings_db_max_size = DATABASE_MAX_SIZE;
static float new_face_embeddings[SIZE_EMBEDDING];
static int state = 0;
/*******************************************************************************
 * Code
 ******************************************************************************/

/*
 * get registration status.
 * */
int registration_state()
{
    return state;
}

/*
 * reset registration status once user added.
 * */
int reset_registration_state()
{
    state = 0;
    return state;
}

/*
 * Get pointer to persons database.
 */
void init_database(face_t * db)
{
    embeddings_db = db;
}

/*
 * Set new persons embeddings.
 */
void set_new_face_embeddings(const float *person_embeddings)
{
    memcpy(new_face_embeddings, person_embeddings, sizeof(new_face_embeddings));
}

/*
 * This function is used to delete a person from the database.
 * @param Name  Name of the Person to be deleted from the database.
 * @param size size of the database.
 * @retval size of the databse.
 */
static int delete_person (char* Name, int size)
{
    int i = 0, j = 0, z = 0;
    int position = 0;

    if (size > embeddings_db_max_size)
    {
        PRINTF("[ERR] - Database size (%d) exceeds maximum size (%d)\r\n", size, embeddings_db_max_size);
        return 1;
    }

    for(i = 0; i < size ;i++)
    {
        if(strcmp(embeddings_db[i].name, Name) == 0)
        {
            position = i;
            PRINTF("person found at %d \r\n", position);
            for(j = position; j < size-1;j++)
            {
                strcpy(embeddings_db[j].name , embeddings_db[j+1].name);
                for (z = 0; z < SIZE_EMBEDDING ; z++)
                {
                    embeddings_db[j].embedding[z] = embeddings_db[j+1].embedding[z];
                }
            }

            /* clear last name and embeddings */
            strcpy(embeddings_db[size-1].name , "\0");
            for (z = 0; z < SIZE_EMBEDDING ; z++)
                embeddings_db[size-1].embedding[z] = 0.0f;

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
static int calculate_size(face_t * face)
{
    int num_faces = 0;

    while ((num_faces < embeddings_db_max_size) && (face[num_faces].name[0] != '\0')){
        num_faces++;
    }

    return num_faces;
}

/*
 * This function is used to add a person to the database
 * @param Name  Name of the Person to be added to the database.
 * @param position Position where to add the person in the database
 */
static void add_person (char* Name, int position)
{
    if (position >= embeddings_db_max_size)
    {
        PRINTF("[ERR] - Position in DB (%d) is out of bounds (%d\r\n", position, embeddings_db_max_size);
        return;
    }

    strcpy(embeddings_db[position].name, Name);
    PRINTF("position:%d\r\n",position);

    for (int i = 0; i < SIZE_EMBEDDING ; i++) {
        embeddings_db[position].embedding[i] = new_face_embeddings[i];
    }
}

/*
 * Add new person to database without using shell
 */
int database_add(char* name)
{
    const int database_size = calculate_size(embeddings_db);

    if (database_size >= embeddings_db_max_size)
    {
        PRINTF("[ERR] - Current data base size is %d\r\n", embeddings_db_max_size);
        PRINTF("[ERR] - Database is full, cannot add more persons. Please increse the size of the db\r\n");
        return 0;
    }

    PRINTF("size %d\r\n", database_size + 1);
    add_person(name,database_size);
    PRINTF("Person added name %s\r\n", embeddings_db[database_size].name);

    return 1;
}

/*
 * Remove person from database without using shell
 */
int database_delete(char* name)
{
    int database_size = calculate_size(embeddings_db);

    delete_person(name, database_size);

    PRINTF("%s deleted from database. \r\n", name);

    return 1;
}

/*
 * Delete all persons from database
 */
void database_delete_all(void)
{
    int crt_face_idx = 0, z = 0;

    while ((crt_face_idx < embeddings_db_max_size) && (embeddings_db[crt_face_idx].name[0] != '\0')) 
    {
        strcpy(embeddings_db[crt_face_idx].name , "\0");
        for (z = 0; z < SIZE_EMBEDDING ; z++)
            embeddings_db[crt_face_idx].embedding[z] = 0.0f;
        crt_face_idx++;
    }
}
