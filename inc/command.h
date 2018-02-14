/*
 *  Code 'n Load command header.
 *  Copyright (C) 2018  Pablo Ridolfi - Code 'n Load
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _COMMAND_H_
#define _COMMAND_H_

/*==================[inclusions]=============================================*/

/*==================[macros]=================================================*/

/*==================[typedef]================================================*/

/** @brief command callback definition
 *
 * @param cmd     expected command string
 * @param rsp     response buffer
 * @param rsp_len response buffer length
 * @return 0 if command executed successfully, other value on error
 */
typedef int (*cnl_cmd_callback)(const char * cmd, char * rsp, size_t rsp_len);

/** @brief command structure declaration
 */
typedef struct {
  const char * cmd;    /**< expected command string */
  cnl_cmd_callback cb; /**< callback to be executed */
} cnl_cmd_t;

/*==================[external data declaration]==============================*/

/*==================[external functions declaration]=========================*/

/** @brief execute command
 *
 * @param cmd    command string
 * @param client file descriptor where to send command output
 * @return 0 if command executed successfully, other value on error
 */
int cnl_command(const char * cmd, int client);

/** @brief run system command
 *
 * @param cmd     command to be run using popen
 * @param rsp     command response buffer
 * @param rsp_len command response buffer length
 * @return the return value of the process, -1 on error
 */
int cnl_run(const char * cmd, char * rsp, size_t rsp_len);

/*==================[end of file]============================================*/
#endif /* #ifndef _COMMAND_H_ */
