/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EMBEDDING_DB_INFOS_H
#define EMBEDDING_DB_INFOS_H

#include APP_TFLITE_MOBILEFACENET_INFO

#define MAX_NAME_SIZE   32
#define DATABASE_MAX_SIZE 100

typedef struct _face_t {
    char name[MAX_NAME_SIZE+1];
    float embedding[SIZE_EMBEDDING];
} face_t;

#endif /* EMBEDDING_DB_INFOS_H */
