/*
 * Copyright 2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* @brief This test application is testing following elements:
 * static image -> TensorFlow Lite model SCRFD_KPS_500M.
 * The model performs face detection and landmarks
 * the model output is displayed on UART console by application.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "string.h"
#include "stdbool.h"
#include "atomic.h"

/* NXP includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "app.h"

#include "hal_debug.h"

/* MPP includes */
#include "mpp_api.h"
#include "mpp_config.h"

#include "../../models/utils.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define NUM_BOXES_MAX APP_MAX_BOXES/* max nb of boxes to filter */
#define TEST_MAX_BOXES 10

/*******************************************************************************
 * Variables declaration
 ******************************************************************************/
static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void print_test_result(const char* test_name, bool passed);
static void print_test_summary(void);
static box_data create_box(int16_t left, int16_t top, int16_t right, int16_t bottom, float score, int16_t label);
static void clear_boxes(box_data boxes[], int32_t max_boxes);
static int count_valid_boxes(box_data boxes[], int32_t max_boxes);

// Test functions for nms_insert_box
static void test_nms_insert_box_empty_array(void);
static void test_nms_insert_box_single_box(void);
static void test_nms_insert_box_score_ordering(void);
static void test_nms_insert_box_no_overlap_different_scores(void);
static void test_nms_insert_box_high_overlap_suppress_lower_score(void);
static void test_nms_insert_box_high_overlap_replace_lower_score(void);
static void test_nms_insert_box_low_overlap_keep_both(void);
static void test_nms_insert_box_multiple_boxes_complex(void);
static void test_nms_insert_box_array_limit(void);
static void test_nms_insert_box_identical_boxes(void);
static void test_nms_insert_box_edge_case_thresholds(void);
static void test_nms_insert_box_cascade_removal(void);

// Test functions for nms
static void test_nms_empty_array(void);
static void test_nms_single_box(void);
static void test_nms_score_filtering(void);
static void test_nms_no_overlap_different_scores(void);
static void test_nms_high_overlap_suppress_lower_score(void);
static void test_nms_low_overlap_keep_both(void);
static void test_nms_multiple_boxes_complex(void);
static void test_nms_identical_boxes(void);
static void test_nms_edge_case_thresholds(void);
static void test_nms_different_labels(void);
static void test_nms_mixed_scores_and_overlaps(void);
static void test_nms_score_ordering_preservation(void);

/*******************************************************************************
 * Code
 ******************************************************************************/

static void print_test_result(const char* test_name, bool passed)
{
    test_count++;
    if (passed) {
        test_passed++;
        PRINTF("[PASS] %s\r\n", test_name);
    } else {
        test_failed++;
        PRINTF("[FAIL] %s\r\n", test_name);
    }
}

static void print_test_summary(void)
{
    PRINTF("\r\n=== TEST SUMMARY ===\r\n");
    PRINTF("Total tests: %d\r\n", test_count);
    PRINTF("Passed: %d\r\n", test_passed);
    PRINTF("Failed: %d\r\n", test_failed);
    PRINTF("Success rate: %d%%\r\n", (test_passed * 100) / test_count);
}

static box_data create_box(int16_t left, int16_t top, int16_t right, int16_t bottom, float score, int16_t label)
{
    box_data box = {0};
    box.left = left;
    box.top = top;
    box.right = right;
    box.bottom = bottom;
    box.score = score;
    box.label = label;
    box.area = 0; // Will be computed by nms functions
    return box;
}

static void clear_boxes(box_data boxes[], int32_t max_boxes)
{
    memset(boxes, 0, sizeof(box_data) * max_boxes);
}

static int count_valid_boxes(box_data boxes[], int32_t max_boxes)
{
    int count = 0;
    for (int i = 0; i < max_boxes; i++) {
        if (boxes[i].area != 0 && boxes[i].score > 0) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// NMS_INSERT_BOX TESTS
// ============================================================================

static void test_nms_insert_box_empty_array(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    box_data new_box = create_box(10, 10, 50, 50, 0.9f, 1);
    int32_t result = nms_insert_box(boxes, new_box, 0, 0.5f, TEST_MAX_BOXES);
    
    bool passed = (result == 1) && 
                  (boxes[0].left == 10) && (boxes[0].top == 10) &&
                  (boxes[0].right == 50) && (boxes[0].bottom == 50) &&
                  (boxes[0].score == 0.9f) && (boxes[0].label == 1) &&
                  (boxes[0].area > 0);
    
    print_test_result("nms_insert_box: Insert into empty array", passed);
}

static void test_nms_insert_box_single_box(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    // Insert first box
    box_data box1 = create_box(10, 10, 50, 50, 0.8f, 1);
    int32_t count = nms_insert_box(boxes, box1, 0, 0.5f, TEST_MAX_BOXES);
    
    // Insert second box with higher score, no overlap
    box_data box2 = create_box(60, 60, 100, 100, 0.9f, 1);
    count = nms_insert_box(boxes, box2, count, 0.5f, TEST_MAX_BOXES);
    
    bool passed = (count == 2) &&
                  (boxes[0].score == 0.9f) && // Higher score should be first
                  (boxes[1].score == 0.8f);
    
    print_test_result("nms_insert_box: Insert with score ordering", passed);
}

static void test_nms_insert_box_score_ordering(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    // Insert boxes with different scores, no overlap
    box_data box1 = create_box(10, 10, 30, 30, 0.5f, 1);
    box_data box2 = create_box(40, 40, 60, 60, 0.8f, 1);
    box_data box3 = create_box(70, 70, 90, 90, 0.3f, 1);
    box_data box4 = create_box(100, 100, 120, 120, 0.9f, 1);
    
    int32_t count = 0;
    count = nms_insert_box(boxes, box1, count, 0.5f, TEST_MAX_BOXES);
    count = nms_insert_box(boxes, box2, count, 0.5f, TEST_MAX_BOXES);
    count = nms_insert_box(boxes, box3, count, 0.5f, TEST_MAX_BOXES);
    count = nms_insert_box(boxes, box4, count, 0.5f, TEST_MAX_BOXES);
    
    bool passed = (count == 4) &&
                  (boxes[0].score == 0.9f) &&
                  (boxes[1].score == 0.8f) &&
                  (boxes[2].score == 0.5f) &&
                  (boxes[3].score == 0.3f);
    
    print_test_result("nms_insert_box: Score ordering with multiple boxes", passed);
}

static void test_nms_insert_box_no_overlap_different_scores(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    // Insert non-overlapping boxes
    box_data box1 = create_box(0, 0, 10, 10, 0.7f, 1);
    box_data box2 = create_box(20, 20, 30, 30, 0.6f, 1);
    
    int32_t count = 0;
    count = nms_insert_box(boxes, box1, count, 0.5f, TEST_MAX_BOXES);
    count = nms_insert_box(boxes, box2, count, 0.5f, TEST_MAX_BOXES);
    
    bool passed = (count == 2);
    
    print_test_result("nms_insert_box: No overlap, different scores", passed);
}

static void test_nms_insert_box_high_overlap_suppress_lower_score(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    // Insert box with high score
    box_data box1 = create_box(10, 10, 50, 50, 0.9f, 1);
    int32_t count = nms_insert_box(boxes, box1, 0, 0.3f, TEST_MAX_BOXES);
    
    // Try to insert overlapping box with lower score
    box_data box2 = create_box(15, 15, 55, 55, 0.7f, 1); // High overlap
    count = nms_insert_box(boxes, box2, count, 0.3f, TEST_MAX_BOXES);
    
    bool passed = (count == 1) && (boxes[0].score == 0.9f);
    
    print_test_result("nms_insert_box: High overlap, suppress lower score", passed);
}

static void test_nms_insert_box_high_overlap_replace_lower_score(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    // Insert box with lower score
    box_data box1 = create_box(10, 10, 50, 50, 0.6f, 1);
    int32_t count = nms_insert_box(boxes, box1, 0, 0.3f, TEST_MAX_BOXES);
    
    // Insert overlapping box with higher score
    box_data box2 = create_box(15, 15, 55, 55, 0.9f, 1); // High overlap
    count = nms_insert_box(boxes, box2, count, 0.3f, TEST_MAX_BOXES);
    
    bool passed = (count == 1) && (boxes[0].score  == 0.9f) &&
                                   boxes[0].left   == 15 &&
                                   boxes[0].top    == 15 &&
                                   boxes[0].right  == 55 &&
                                   boxes[0].bottom == 55;
    
    print_test_result("nms_insert_box: High overlap, replace with higher score", passed);
}

static void test_nms_insert_box_low_overlap_keep_both(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    // Insert boxes with low overlap
    box_data box1 = create_box(10, 10, 30, 30, 0.8f, 1);
    box_data box2 = create_box(25, 25, 45, 45, 0.7f, 1); // Low overlap
    
    int32_t count = 0;
    count = nms_insert_box(boxes, box1, count, 0.5f, TEST_MAX_BOXES);
    count = nms_insert_box(boxes, box2, count, 0.5f, TEST_MAX_BOXES);
    
    bool passed = (count == 2);
    
    print_test_result("nms_insert_box: Low overlap, keep both boxes", passed);
}

static void test_nms_insert_box_multiple_boxes_complex(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    // Complex scenario with multiple boxes
    box_data box1 = create_box(10, 10, 30, 30, 0.7f, 1);
    box_data box2 = create_box(40, 40, 60, 60, 0.6f, 1);
    box_data box3 = create_box(15, 15, 35, 35, 0.9f, 1); // Overlaps with box1
    box_data box4 = create_box(70, 70, 90, 90, 0.5f, 1);
    
    int32_t count = 0;
    count = nms_insert_box(boxes, box1, count, 0.3f, TEST_MAX_BOXES);
    count = nms_insert_box(boxes, box2, count, 0.3f, TEST_MAX_BOXES);
    count = nms_insert_box(boxes, box3, count, 0.3f, TEST_MAX_BOXES); // Should replace box1
    count = nms_insert_box(boxes, box4, count, 0.3f, TEST_MAX_BOXES);
    
    bool passed = (count == 3) && (boxes[0].score == 0.9f);
    
    print_test_result("nms_insert_box: Complex multiple boxes scenario", passed);
}

static void test_nms_insert_box_array_limit(void)
{
    box_data boxes[3]; // Small array
    clear_boxes(boxes, 3);
    
    int32_t count = 0;
    // Fill array to capacity with non-overlapping boxes
    for (int i = 0; i < 5; i++) {
        box_data box = create_box(i*20, i*20, i*20+10, i*20+10, 0.8f - i*0.1f, 1);
        count = nms_insert_box(boxes, box, count, 0.5f, 3);
    }
    
    bool passed = (count == 3); // Should not exceed array size

    // Verify the final state of the boxes array
    passed = passed && boxes[0].score == 0.8f &&
                       boxes[1].score == 0.7f &&
                       boxes[2].score == 0.6f;

    
    print_test_result("nms_insert_box: Array size limit handling", passed);
}

static void test_nms_insert_box_identical_boxes(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    // Insert identical boxes with different scores
    box_data box1 = create_box(10, 10, 50, 50, 0.7f, 1);
    box_data box2 = create_box(10, 10, 50, 50, 0.9f, 1); // Identical position
    
    int32_t count = 0;
    count = nms_insert_box(boxes, box1, count, 0.5f, TEST_MAX_BOXES);
    count = nms_insert_box(boxes, box2, count, 0.5f, TEST_MAX_BOXES);
    
    bool passed = (count == 1) && (boxes[0].score == 0.9f);
    
    print_test_result("nms_insert_box: Identical boxes, keep higher score", passed);
}

static void test_nms_insert_box_edge_case_thresholds(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    // Test with very low and very high thresholds
    box_data box1 = create_box(10, 10, 30, 30, 0.8f, 1);
    box_data box2 = create_box(20, 20, 40, 40, 0.7f, 1);
    
    int32_t count = 0;
    count = nms_insert_box(boxes, box1, count, 0.0f, TEST_MAX_BOXES); // Very strict
    count = nms_insert_box(boxes, box2, count, 0.0f, TEST_MAX_BOXES);
    
    bool passed_strict = (count == 1); // Should suppress overlapping box
    
    clear_boxes(boxes, TEST_MAX_BOXES);
    count = 0;
    count = nms_insert_box(boxes, box1, count, 1.0f, TEST_MAX_BOXES); // Very lenient
    count = nms_insert_box(boxes, box2, count, 1.0f, TEST_MAX_BOXES);
    
    bool passed_lenient = (count == 2); // Should keep both
    
    print_test_result("nms_insert_box: Edge case thresholds", passed_strict && passed_lenient);
}

static void test_nms_insert_box_cascade_removal(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    // Create a scenario where inserting one box removes multiple existing boxes
    box_data box1 = create_box(10, 10, 30, 30, 0.5f, 1);
    box_data box2 = create_box(35, 35, 55, 55, 0.4f, 1);
    box_data box3 = create_box(60, 60, 80, 80, 0.3f, 1);
    box_data box_new = create_box(5, 5, 80, 80, 0.9f, 1); // Overlaps with all
    
    int32_t count = 0;
    count = nms_insert_box(boxes, box1, count, 0.0f, TEST_MAX_BOXES);
    count = nms_insert_box(boxes, box2, count, 0.0f, TEST_MAX_BOXES);
    count = nms_insert_box(boxes, box3, count, 0.0f, TEST_MAX_BOXES);
    count = nms_insert_box(boxes, box_new, count, 0.0f, TEST_MAX_BOXES);
    
    bool passed = (count == 1) && (boxes[0].score == 0.9f);
    
    print_test_result("nms_insert_box: Cascade removal scenario", passed);
}

// ============================================================================
// NMS TESTS
// ============================================================================

static void test_nms_empty_array(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    nms(boxes, 0, 0.5f, 0.3f);
    
    bool passed = true; // Should not crash
    
    print_test_result("nms: Empty array", passed);
}

static void test_nms_single_box(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    boxes[0] = create_box(10, 10, 50, 50, 0.9f, 1);
    
    nms(boxes, 1, 0.5f, 0.3f);
    
    bool passed = (boxes[0].score == 0.9f) && (boxes[0].area > 0);
    
    print_test_result("nms: Single box above threshold", passed);
}

static void test_nms_score_filtering(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    boxes[0] = create_box(10, 10, 30, 30, 0.9f, 1);  // Above threshold
    boxes[1] = create_box(40, 40, 60, 60, 0.2f, 1);  // Below threshold
    boxes[2] = create_box(70, 70, 90, 90, 0.8f, 1);  // Above threshold
    
    nms(boxes, 3, 0.5f, 0.3f);
    
    bool passed = (boxes[0].score == 0.9f) && (boxes[0].area > 0) &&
                  (boxes[1].score == 0.8f) && (boxes[1].area > 0) && 
                  (boxes[2].area == 0); // should be filtered out
    
    print_test_result("nms: Score filtering", passed);
}

static void test_nms_no_overlap_different_scores(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    boxes[0] = create_box(0, 0, 10, 10, 0.7f, 1);
    boxes[1] = create_box(20, 20, 30, 30, 0.6f, 1);
    boxes[2] = create_box(40, 40, 50, 50, 0.8f, 1);
    
    nms(boxes, 3, 0.5f, 0.3f);
    
    int valid_count = count_valid_boxes(boxes, TEST_MAX_BOXES);
    bool passed = (valid_count == 3); // All should remain
    
    print_test_result("nms: No overlap, different scores", passed);
}

static void test_nms_high_overlap_suppress_lower_score(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    boxes[0] = create_box(10, 10, 50, 50, 0.9f, 1);
    boxes[1] = create_box(15, 15, 55, 55, 0.7f, 1); // High overlap, lower score
    
    nms(boxes, 2, 0.3f, 0.3f);
    
    int valid_count = count_valid_boxes(boxes, TEST_MAX_BOXES);
    bool passed = (valid_count == 1) && (boxes[0].score == 0.9f);
    
    print_test_result("nms: High overlap, suppress lower score", passed);
}

static void test_nms_low_overlap_keep_both(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    boxes[0] = create_box(10, 10, 30, 30, 0.8f, 1);
    boxes[1] = create_box(25, 25, 45, 45, 0.7f, 1); // Low overlap
    
    nms(boxes, 2, 0.5f, 0.3f);
    
    int valid_count = count_valid_boxes(boxes, TEST_MAX_BOXES);
    bool passed = (valid_count == 2);
    
    print_test_result("nms: Low overlap, keep both boxes", passed);
}

static void test_nms_multiple_boxes_complex(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    boxes[0] = create_box(10, 10, 30, 30, 0.7f, 1);
    boxes[1] = create_box(15, 15, 35, 35, 0.9f, 1); // Overlaps with box0, higher score
    boxes[2] = create_box(40, 40, 60, 60, 0.6f, 1); // No overlap
    boxes[3] = create_box(70, 70, 90, 90, 0.5f, 1); // No overlap
    
    nms(boxes, 4, 0.3f, 0.3f);
    
    int valid_count = count_valid_boxes(boxes, TEST_MAX_BOXES);
    // Should keep boxes 1, 2, 3 (box 0 should be suppressed by box 1)
    bool passed = (valid_count == 3);
    
    print_test_result("nms: Complex multiple boxes scenario", passed);
}

static void test_nms_identical_boxes(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    boxes[0] = create_box(10, 10, 50, 50, 0.7f, 1);
    boxes[1] = create_box(10, 10, 50, 50, 0.9f, 1); // Identical position, higher score
    
    nms(boxes, 2, 0.5f, 0.3f);
    
    int valid_count = count_valid_boxes(boxes, TEST_MAX_BOXES);
    bool passed = (valid_count == 1);

    // Check that the box with higher score is retained
    passed = passed && (boxes[0].score == 0.9f);
    
    print_test_result("nms: Identical boxes, keep higher score", passed);
}

static void test_nms_edge_case_thresholds(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    boxes[0] = create_box(10, 10, 30, 30, 0.8f, 1);
    boxes[1] = create_box(20, 20, 40, 40, 0.7f, 1);
    
    // Test with very strict threshold
    nms(boxes, 2, 0.0f, 0.3f);
    
    int valid_count_strict = count_valid_boxes(boxes, TEST_MAX_BOXES);
    bool passed_strict = (valid_count_strict == 1);
    
    // Reset and test with very lenient threshold
    clear_boxes(boxes, TEST_MAX_BOXES);
    boxes[0] = create_box(10, 10, 30, 30, 0.8f, 1);
    boxes[1] = create_box(20, 20, 40, 40, 0.7f, 1);
    
    nms(boxes, 2, 1.0f, 0.3f);
    
    int valid_count_lenient = count_valid_boxes(boxes, TEST_MAX_BOXES);
    bool passed_lenient = (valid_count_lenient == 2);
    
    print_test_result("nms: Edge case thresholds", passed_strict && passed_lenient);
}

static void test_nms_different_labels(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    boxes[0] = create_box(10, 10, 50, 50, 0.8f, 1);
    boxes[1] = create_box(10, 10, 50, 50, 0.7f, 2); // Different label, high overlap
    
    nms(boxes, 2, 0.3f, 0.3f);
    
    int valid_count = count_valid_boxes(boxes, TEST_MAX_BOXES);
    bool passed = (valid_count == 2); // Should keep both due to different labels
    
    print_test_result("nms: Different labels, keep both", passed);
}

static void test_nms_mixed_scores_and_overlaps(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    boxes[0] = create_box(10, 10, 30, 30, 0.9f, 1);
    boxes[1] = create_box(15, 15, 35, 35, 0.8f, 1); // High overlap, should be suppressed
    boxes[2] = create_box(40, 40, 60, 60, 0.7f, 1); // No overlap, should remain
    boxes[3] = create_box(55, 55, 65, 65, 0.6f, 1); // Low overlap with box2, should remain
    boxes[4] = create_box(5, 5, 25, 25, 0.5f, 1);   // High overlap with box0, should be suppressed
    boxes[5] = create_box(100, 100, 120, 120, 0.4f, 1); // No overlap, should remain
    
    nms(boxes, 6, 0.3f, 0.3f);
    
    int valid_count = count_valid_boxes(boxes, TEST_MAX_BOXES);
    // Should keep boxes 0, 2, 3, 5 (boxes 1 and 4 should be suppressed)
    bool passed = (valid_count == 4);

    // check that boxes 0, 2, 3, 5 remain (highest scores)
    passed = passed && boxes[0].score == 0.9f &&
                       boxes[2].score == 0.7f &&
                       boxes[3].score == 0.6f &&
                       boxes[5].score == 0.4f;

    print_test_result("nms: Mixed scores and overlaps", passed);
}

static void test_nms_score_ordering_preservation(void)
{
    box_data boxes[TEST_MAX_BOXES];
    clear_boxes(boxes, TEST_MAX_BOXES);
    
    // Insert boxes in random score order, no overlaps
    boxes[0] = create_box(10, 10, 20, 20, 0.5f, 1);
    boxes[1] = create_box(30, 30, 40, 40, 0.9f, 1);
    boxes[2] = create_box(50, 50, 60, 60, 0.3f, 1);
    boxes[3] = create_box(70, 70, 80, 80, 0.7f, 1);
    
    nms(boxes, 4, 0.5f, 0.2f);
    
    // After NMS, boxes should be sorted by score (highest first)
    bool passed = (boxes[0].score >= boxes[1].score) &&
                  (boxes[1].score >= boxes[2].score) &&
                  (boxes[2].score >= boxes[3].score);
    
    print_test_result("nms: Score ordering preservation", passed);
}

/*!
 * @brief Application entry point.
 */
int main(int argc, char *argv[])
{
    /* Init board hardware. */
    BOARD_Init();

    PRINTF("****** TEST test_non_max_suppression ******\r\n");
    PRINTF("Testing NMS functions...\r\n\r\n");

    PRINTF("=== Testing nms_insert_box function ===\r\n");
    // Run nms_insert_box tests
    test_nms_insert_box_empty_array();
    test_nms_insert_box_single_box();
    test_nms_insert_box_score_ordering();
    test_nms_insert_box_no_overlap_different_scores();
    test_nms_insert_box_high_overlap_suppress_lower_score();
    test_nms_insert_box_high_overlap_replace_lower_score();
    test_nms_insert_box_low_overlap_keep_both();
    test_nms_insert_box_multiple_boxes_complex();
    test_nms_insert_box_array_limit();
    test_nms_insert_box_identical_boxes();
    test_nms_insert_box_edge_case_thresholds();
    test_nms_insert_box_cascade_removal();

    PRINTF("\r\n=== Testing nms function ===\r\n");
    // Run nms tests
    test_nms_empty_array();
    test_nms_single_box();
    test_nms_score_filtering();
    test_nms_no_overlap_different_scores();
    test_nms_high_overlap_suppress_lower_score();
    test_nms_low_overlap_keep_both();
    test_nms_multiple_boxes_complex();
    test_nms_identical_boxes();
    test_nms_edge_case_thresholds();
    test_nms_different_labels();
    test_nms_mixed_scores_and_overlaps();
    test_nms_score_ordering_preservation();

    print_test_summary();

    PRINTF("\r\n****** TEST COMPLETED ******\r\n");

    return 0;
}
