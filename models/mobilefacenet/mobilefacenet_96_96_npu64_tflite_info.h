/*
 * Copyright 2025 NXP
 * Copyright 2022 xuehao.ma
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file provides information about the TFLite Mobilefacenet model, such as width, heigth and color format.
   Other parameters include input mean and std, output scale and zero point, recognition threshold,
   the number of channels and the size of embeddings.
*/

#ifndef MOBILEFACENET_96_96_TFLITE_INFO_H
#define MOBILEFACENET_96_96_TFLITE_INFO_H

#define MOBILEFACENET_NAME "Mobilefacenet fully quantized model"

/*
 * mean and std will be used to get input values in the model input data range.
 * Mobilefacenet performs the normalization of input using the two first layers(Sub and Mul),
 * no need to perform extra-normalization .
 * */

#define MOBILEFACENET_INPUT_MEAN      0
#define MOBILEFACENET_INPUT_STD       1
#define MOBILEFACENET_WIDTH           96
#define MOBILEFACENET_HEIGHT          96
#define MOBILEFACENET_FORMAT       	  MPP_PIXEL_BGR
/*
 * Threshold is obtained from model evaluation on faces dataset to acheive a FAR target of 0.0001.
 * It is computed using the squared euclidean distance metric.
 */
#define MATCH_THRESHOLD			      1.394f
/*
 * OUTPUT_SCALE and OUTPUT_ZERO_POINT are the model quantization parameters and can be found
 by visualizing the model using https://netron.app/
 */
#define OUTPUT_SCALE                  0.014942022040486336f
#define OUTPUT_ZERO_POINT             1.0f
/*
 * Size of output embeddings.
 */
#define SIZE_EMBEDDING                256

#endif /* MOBILEFACENET_96_96_TFLITE_INFO_H */
