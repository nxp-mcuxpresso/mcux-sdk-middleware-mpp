/*
 * Copyright 2021-2026 NXP
 *
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * API functions public header
 */
#ifndef _MPP_API_H
#define _MPP_API_H

#include "mpp_api_types.h"
#include "mpp_config.h"

#if (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1) && (defined MCMGR_USED)
#include "mcmgr.h"
#endif /* (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1) && (defined MCMGR_USED) */

/**
 * @defgroup MPP_API MPP API
 *
 * This section provides the detailed documentation for the MCU Media Processing Pipeline API
 *
 * @{
 */

/**
 * Pipeline initialization
 *
 * This function initializes the library and its data structures. <br>
 * It must be called before any other function of the API is called.
 *
 * @param [in] params API global parameters
 * @return \ref return_codes
 *
 */
int mpp_api_init(mpp_api_params_t *params);

/**
 * Basic pipeline creation
 *
 * This function returns a handle to the pipeline.
 *
 * @param [in] params pipeline parameters
 * @param [out] ret return code (0 - success, non-zero - error)
 * @return handle to the pipeline if success, NULL if there is an error.
 *
 */
mpp_t mpp_create(mpp_params_t *params, int *ret);

/**
 * Camera addition
 *
 * This function adds a camera to the pipeline.
 *
 * @param [in] mpp input pipeline
 * @param [in] name camera driver name
 * @param [in] params parameters to be configured on the camera
 * @param [out] elem_h element handle in pipeline
 * @return \ref return_codes
 */
int mpp_camera_add(mpp_t mpp, const char* name, mpp_camera_params_t *params, mpp_elem_handle_t *elem_h);

/**
 * Static image addition
 *
 * @param [in] mpp input pipeline
 * @param [in] params static image parameters
 * @param [in] addr image buffer
 * @param [out] elem_h element handle in pipeline
 * @return \ref return_codes
 *
 * @pre
 * - Image buffer allocation/free is the responsibility of the user.
 *
 */
int mpp_static_img_add(mpp_t mpp, mpp_img_params_t *params, void *addr, mpp_elem_handle_t *elem_h);

/**
 * Source file addition
 *
 * @param [in] mpp input pipeline
 * @param [in] params file parameters
 * @param [in] addr file buffer
 * @param [out] elem_h element handle in pipeline
 * @return \ref return_codes
 *
 */
int mpp_filesrc_add(mpp_t mpp, mpp_filesrc_params_t *params, void *addr, mpp_elem_handle_t *elem_h);

/**
 * Sink file addition
 *
 * @param [in] mpp input pipeline
 * @param [in] params file parameters
 * @param [out] elem_h element handle in pipeline
 * @return \ref return_codes
 *
 */
int mpp_filesink_add(mpp_t mpp, mpp_filesink_params_t *params, mpp_elem_handle_t *elem_h);

/**
 * Source RTSP addition
 *
 * @param [in] mpp input pipeline
 * @param [in] params RTSP parameters
 * @param [out] elem_h element handle in pipeline
 * @return \ref return_codes
 *
 */
int mpp_rtspsrc_add(mpp_t mpp, mpp_rtspsrc_params_t *params, mpp_elem_handle_t *elem_h);

/**
 * Sink RTSP addition
 *
 * @param [in] mpp input pipeline
 * @param [in] params RTSP parameters
 * @param [out] elem_h element handle in pipeline
 * @return \ref return_codes
 *
 */
int mpp_rtspsink_add(mpp_t mpp, mpp_rtspsink_params_t *params, mpp_elem_handle_t *elem_h);

/**
 * Multi core source addition
 *
 * This function adds a multi core source to the pipeline.
 *
 * @param [in] mpp input pipeline
 * @param [in] params parameters that are configured on the multi core source
 * @param [out] elem_h element handle in pipeline
 * @return \ref return_codes
 */
int mpp_mc_source_add(mpp_t mpp, mpp_mc_params_t *params, mpp_elem_handle_t *elem_h);

/**
 * Display addition
 *
 * This function adds a display to the pipeline.
 *
 * @param [in] mpp input pipeline
 * @param [in] name display driver name
 * @param [in] params parameters that are configured on the display
 * @return \ref return_codes
 */
int mpp_display_add(mpp_t mpp, const char *name, mpp_display_params_t *params);

/**
 * Multi core sink addition
 *
 * This function adds a multi core sink to the pipeline.
 *
 * @param [in] mpp input pipeline
 * @param [in] params parameters that are configured on the multi core sink
 * @return \ref return_codes
 */
int mpp_mc_sink_add(mpp_t mpp, mpp_mc_params_t *params);

/**
 * Null sink addition
 *
 * This function adds a null-type sink to the pipeline. <br>
 * After this call pipeline is closed and no further elements
 * can be added. Input frames are discarded.
 *
 * @param [in] mpp input pipeline
 * @return \ref return_codes
 */
int mpp_nullsink_add(mpp_t mpp);

/**
 * Get output buffer address
 *
 * This function returns the address of an output buffer for a given element.
 *
 * @param [in] elem element handle in pipeline
 * @param [in] index output buffer index (must be less than MAX_OUTPUT_PORTS and element's nb_out_buf)
 * @return pointer to the output buffer address if successful, NULL if element is invalid or index is out of range
 */
void *mpp_get_output_buff_address(mpp_elem_handle_t elem, uint32_t index);

/**
 * Get input buffer address
 *
 * This function returns the address of an input buffer for a given element.
 *
 * @param [in] elem element handle in pipeline
 * @param [in] index input buffer index (must be less than MAX_INPUT_PORTS and element's nb_in_buf)
 * @return pointer to the input buffer address if successful, NULL if element is invalid or index is out of range
 */
void *mpp_get_input_buff_address(mpp_elem_handle_t elem, uint32_t index);

/**
 * Add processing element (single input, single output)
 *
 * This function adds an element to the pipeline.
 *
 * Available elements are:
 * - 2D image processing
 * - ML inference engine
 * - Labeled rectangle
 * - Compositor
 *
 * @param [in] mpp input pipeline
 * @param [in] id element id
 * @param [in] params element parameters
 * @param [out] elem_h element handle in pipeline
 * @return \ref return_codes
 */
int mpp_element_add(mpp_t mpp, mpp_element_id_t id, mpp_element_params_t *params, mpp_elem_handle_t *elem_h);

/**
 * Pipeline multiplication
 *
 * @param [in] mpp input pipeline
 * @param [in] num number of output pipeline
 * @param [in] params split mpp parameters
 * @param [out] out_list list of output pipelines
 * @return \ref return_codes
 *
 * @pre
 * - _out_list_ array must contain at least _num_ elements.
 */
int mpp_split(mpp_t mpp, unsigned int num, mpp_params_t *params, mpp_t *out_list);

/**
 * Link a newly built pipeline as a live branch off an existing element
 * in a running pipeline.
 *
 * The new pipeline must have been fully constructed and closed before this
 * call. Its exec_flag (set at mpp_create time) determines which heap it
 * joins. The pipeline is started immediately (equivalent to mpp_start with
 * last=false) and begins receiving frames from hook_elem on the next
 * execution cycle.
 *
 * @param [in] hook_elem_h handle of the element in the running pipeline to
 *                         branch off from
 * @param [in] new_mpp     handle of the closed pipeline to attach as a branch
 * @return \ref return_codes
 *
 * @pre
 * - hook_elem_h must belong to a running pipeline
 * - new_mpp must be in MPP_CLOSED state and not running
 * - If new_mpp->first_elem does not own its input buffer, the hook element
 *   output buffer must already be allocated
 */
int mpp_link_branch(mpp_elem_handle_t hook_elem_h, mpp_t new_mpp);

/**
 * Put next elements processing in background
 *
 * @param [in] mpp input pipeline
 * @param [in] params new mpp parameters (exec_flag must be MPP_EXEC_PREEMPT)
 * @param [out] out_mpp output pipeline
 * @return \ref return_codes
 */
int mpp_background(mpp_t mpp, mpp_params_t *params, mpp_t *out_mpp);

/**
 * Add processing element with per-pass parameter sets (single input, single output)
 *
 * This function adds an element configured to run with a different set of
 * parameters on each pass. params_array[i] is selected for pass index i.
 * If the effective pass count exceeds n_params, the last entry is reused.
 *
 * Available elements are the same as for mpp_element_add.
 *
 * @param [in] mpp          input pipeline
 * @param [in] id           element id
 * @param [in] params_array array of element parameters, one per pass (must have at least 1 entry)
 * @param [in] n_params     number of entries in params_array
 * @param [out] elem_h      element handle in pipeline
 * @return \ref return_codes
 */
int mpp_multipass_element_add(mpp_t mpp, mpp_element_id_t id,
                          mpp_element_params_t *params_array,
                          uint32_t n_params,
                          mpp_elem_handle_t *elem_h);

/**
 * Set the number of passes per source frame for a pipeline branch
 *
 * This function sets how many times the pipeline branch is executed per
 * source frame. A value of 0 is invalid and returns MPP_INVALID_PARAM;
 * n_passes must be at least 1.
 * Can be called before or after mpp_start to update the pass count at runtime.
 * Sub-branches that share the same execution heap inherit the pass count
 * from the topmost ancestor whose n_passes is non-zero; a sub-branch whose
 * own n_passes is 0 (unset) falls back to that inherited value.
 *
 * @param [in] mpp      pipeline branch handle
 * @param [in] n_passes desired pass count (must be >= 1)
 * @return \ref return_codes
 */
int mpp_set_n_passes(mpp_t mpp, uint32_t n_passes);

/**
 * Update element parameters
 *
 * @param [in] mpp      input pipeline
 * @param [in] elem_h   element handle in the pipeline.
 * @param [in] params   new element parameters
 * @param [in] force_update  force the pipeline to run even though 
 *         there is no input frame update for processing elements after update.
 *         If the force_update flag was already requested before, the current value is ignored
 * @return \ref return_codes
 */
int mpp_element_update(mpp_t mpp, mpp_elem_handle_t elem_h, mpp_element_params_t *params, bool force_update);

/**
 * Update element parameters for multiple pipeline passes
 *
 * @param [in] mpp          input pipeline
 * @param [in] elem_h       element handle in the pipeline
 * @param [in] params_array array of element parameters, one entry per pass
 * @param [in] n_params     number of entries in params_array
 * @param [in] force_update force the pipeline to run even though
 *         there is no input frame update for processing elements after update.
 *         If the force_update flag was already requested before, the current value is ignored
 * @return \ref return_codes
 */
int mpp_multipass_element_update(mpp_t mpp, mpp_elem_handle_t elem_h,
                             mpp_element_params_t *params_array, uint32_t n_params,
                             bool force_update);

/**
 * Check if the pipeline is currently running
 *
 * @param [in] mpp      input pipeline
 * @return true if pipeline is in running state, else false
 */
bool mpp_is_running(mpp_t mpp);

/**
 * Start pipeline
 *
 * When called with last=0, this function prepares the branch of the pipeline
 * specified with mpp. <br>
 * When called with last!=0, this function starts the data flow of the pipeline. <br>
 * Data flow should start after all the branches of the pipeline have been prepared.
 *
 * @param [in] mpp pipeline branch handle to start/prepare
 * @param [in] last if non-zero start pipeline processing.
 *         No further start call is possible thereafter.
 * @param [in] force_update force the pipeline to run even though 
 *         there is no input frame update for processing elements.
 *         If the force_update flag was already requested before, the current value is ignored
 * @return \ref return_codes
 */
int mpp_start(mpp_t mpp, int last, bool force_update);

/**
 * Stop a branch of the pipeline
 *
 * This function stops the data processing and peripherals of a pipeline branch.
 *
 * @param [in] mpp pipeline branch to stop
 * @return \ref return_codes
 */
int mpp_stop(mpp_t mpp);

/**
 * Force the update of a branch of the pipeline
 *
 * This function forces an update of the branch of the pipeline even if
 *          there is no new input frame.
 *
 * @param [in] mpp pipeline branch to set the flag force_update to true
 * @return \ref return_codes
 */
int mpp_force_update(mpp_t mpp);

#if (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1) && (defined MCMGR_USED)
/**
 * @brief Remote API event handler callback
 * 
 * Called by MCMGR when a remote core signals readiness for API communication.
 * Stores the event in the database for later retrieval during initialization.
 * 
 * @param coreNum Core number that triggered the event
 * @param eventData Event data payload
 * @param context User context (unused)
 */
void mpp_mc_remote_event_handler(mcmgr_core_t coreNum, uint16_t eventData, void *context);
#endif /* (defined HAL_ENABLE_MULTICORE) && (HAL_ENABLE_MULTICORE == 1) && (defined MCMGR_USED) */

/**
 * Enable statistics collection
 *
 * This function enables statistics collection for a given group.
 * Statistics collection is disabled by default after API initialization.
 * Calling this function when stats are enabled has no effect.
 *
 * @param [in]  grp statistics group
 */
void mpp_stats_enable(mpp_stats_grp_t grp);

/**
 * Disable statistics collection
 *
 * This function disables statistics collection for a given group.
 * Calling this function when stats are disabled has no effect.
 * This function is used to ensure stats are not updated while
 * application tasks use the stats structures.
 *
 * @param [in] grp statistics group
 */
void mpp_stats_disable(mpp_stats_grp_t grp);


/**
 * Get MPP version
 *
 * @return pointer to the MPP version string
 *
 */
char* mpp_get_version(void);

#ifdef BOOT_SECONDARY_CORE
/*
 * Boot secondary core using multi-core manager middleware
 *
 * @return pointer to the MCMGR remote event data
 *
 */
volatile uint16_t *mpp_boot_secondary_core(void);

#ifdef MCMGR_USED
/*
 * Signal secondary core ready and initialize RPMsg
 *
 * @return pointer to the created RPMsg instance
 *
 */
struct rpmsg_lite_instance *mpp_secondary_core_rpmsg_init(void);
#endif /* MCMGR_USED */
#endif /* BOOT_SECONDARY_CORE */

#ifdef RPMSG_USED
/* 
 * Initialize RPMsg module
 *
 * @return: pointer to the created RPMsg instance
 *
 */
void *mpp_init_rpmsg(void);
#endif /* RPMSG_USED */

/**
 * Initialize the storage subsystem
 *
 * @return MPP_SUCCESS on success, MPP_ERROR on failure
 */
int mpp_storage_init(void);

/**
 * Get free and total storage space
 *
 * @param[out] free_bytes Pointer to store the free space in bytes
 * @param[out] total_bytes Pointer to store the total space in bytes
 * @return MPP_SUCCESS on success, MPP_ERROR on failure
 */
int mpp_storage_get_free_space(uint64_t *free_bytes, uint64_t *total_bytes);

/**
 * Initialize Ethernet network interface
 *
 * This function initializes the Ethernet network interface with the specified
 * IP address, netmask, and gateway configuration.
 *
 * @param [in] ip_addr IP address as array of 4 octets (e.g., {192, 168, 0, 102})
 * @param [in] netmask Network mask as array of 4 octets (e.g., {255, 255, 255, 0})
 * @param [in] gateway Gateway address as array of 4 octets (e.g., {192, 168, 0, 100})
 * @return 0 on success, non-zero on error
 */
int mpp_eth_netif_init(uint8_t ip_addr[4], uint8_t netmask[4], uint8_t gateway[4]);

/**
 * Destroy a stopped pipeline and free all its resources.
 * The pipeline must not be in MPP_RUNNING state.
 * All HAL device deinit functions are called and all memory allocated
 * for elements, device structs, and output buffer descriptors is freed.
 *
 * @param[in] mpp handle to the pipeline to destroy
 * @return MPP_SUCCESS on success, MPP_ERROR if the pipeline is still running,
 *         MPP_INVALID_PARAM if mpp is NULL.
 */
int mpp_destroy(mpp_t mpp);

/**
 * Destroy the three pipeline control tasks created by mpp_api_init().
 * Also destroys the associated event groups, semaphore, and stats mutexes.
 * May only be called after all pipelines have been destroyed (prio lists empty).
 *
 * @return MPP_SUCCESS on success, MPP_ERROR if any pipeline is still registered.
 */
int mpp_api_ctl_tasks_destroy(void);

/**
 * Print the current free heap size to the log.
 * Useful to call before and after mpp_destroy() to verify full cleanup.
 * Implemented for FreeRTOS only; prints a notice on other OSes.
 */
void mpp_print_heap(void);


/**
 * Get the current free heap size.
 *
 * @return The amount of free heap memory in bytes
 */
uint32_t mpp_get_free_heap(void);

/** @}*/

#endif
