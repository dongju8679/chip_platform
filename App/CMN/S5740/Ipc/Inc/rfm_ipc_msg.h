/* PLACEHOLDER - paste in the original (Ipc/Inc/rfm_ipc_msg.h) and apply only
 * the fix below.
 * (It contains address macros such as RFM_IPC_BUFFER_*_ADDR, so it is not
 *  reconstructed from memory.)
 *   ipc_ReceiveRfmMsgb  ->  ipc_ReceiveRfmMsg   (the extern declaration at the
 *                                                very end; stray 'b' typo)
 *   CRLF -> LF.
 * Note: the definition of MAX_RFM_IPC_MSG_SIZE lives in this file (used by
 * sender.c).
 */
