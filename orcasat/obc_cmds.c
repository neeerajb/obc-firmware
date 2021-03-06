/* Contains the available command line arguments
 *
 * Created: May 30 2017
 * Refactored code from sfu_uart.* by Seleena
 *
 */
#include <assert.h>

#include "obc_uart.h"
#include "obc_cmds.h"
#include "obc_scheduler.h"
#include "obc_state.h"
#include "obc_utils.h"
#include "obc_rtc.h"
#include "sun_sensor.h"
#include "obc_task_radio.h"
#include "deployables.h"
#include "obc_fs_structure.h"
#include "flash_mibspi.h"
#include "obc_gps.h"

struct subcmd_opt {
	const char *name;
	const uint8_t subcmd_id;
	const char *info;
};

/**
 * The following fields are used mainly to facilitate UART debugging:
 * 		- name
 * 		- subcmds
 * 		- num_subcmds
 * Word aligned.
 */
struct cmd_opt {
	const char *name;
	int8_t (*const func)(const CMD_t *cmd);
	const struct subcmd_opt *subcmds;
	const uint8_t cmd_id;
	const uint8_t num_subcmds;
};

int sentinel = 0x89abcdef;
int test_const_size = HASH("test");

/**
 * Help command.
 */
static const struct subcmd_opt CMD_HELP_OPTS[] = {
		{
				.subcmd_id	= CMD_HELP_NONE,
				.name		= "",
				.info		= "Available commands (type `help <cmd>` for more information):\n"
							  "  help  -- Show this help\n"
							  "  get   -- Get various metrics\n"
							  "  exec  -- Execute various actions\n"
							  "  rf    -- RF-related commands\n"
							  "  task  -- Task-related commands\n"
							  "  sched -- Schedule-related commands\n"
							  "  state -- State-related commands\n"
							  "  ack   -- Acks.\n"
						      "  wd    -- Disable watchdog.\n"
						      "  deploy -- Inhibit deployment.\n"
						      "  file   -- File-related commands\n"
		},
		{
				.subcmd_id	= CMD_HELP_GET,
				.name		= "get",
				.info		= "Get various metrics\n"
							  "  tasks   -- Show status of tasks (state, priority, stack, etc)\n"
							  "  runtime -- Show per-task CPU utilization\n"
							  "  heap    -- Show size of free heap\n"
							  "  minheap -- Show lowest size of free heap ever reached\n"
							  "  types   -- Show size of various types (debugging)\n"
							  "	 epoch   -- Show current OBC epoch\n"
		},
		{
				.subcmd_id	= CMD_HELP_EXEC,
				.name		= "exec",
				.info		= "Execute various actions\n"
							  "  rf -- Initialize the radio\n"
		},
		{
				.subcmd_id	= CMD_HELP_RF,
				.name		= "rf",
				.info		= "RF-related commands (none)"
		},
		{
				.subcmd_id	= CMD_HELP_TASK,
				.name		= "task",
				.info		= "Task-related commands\n"
							  "  resume [TASK_ID]\n"
							  "    Resume the given task ID\n"
							  "    TODO: TASK_IDs to be listed here\n"
							  "  suspend [TASK_ID]\n"
							  "    Suspend the given task ID\n"
		},
		{
				.subcmd_id	= CMD_HELP_SCHED,
				.name		= "sched",
				.info		= "Schedule-related commands\n"
							  "  add [HEX DATA]\n"
							  "    Add an event to be scheduled\n"
							  "  remove [INDEX]\n"
							  "    Remove an event from the scheduler\n"
							  "  show\n"
							  "    Show scheduled events\n"
		},
		{
				.subcmd_id	= CMD_HELP_STATE,
				.name		= "state",
				.info		= "State-related commands\n"
							  "  get\n"
							  "    Show current state\n"
							  "  set [HEX DATA]\n"
							  "    Set the current state\n"
							  "  prev\n"
							  "    Show previous state\n"
							  "  entry\n"
							  "    Show time we entered current state\n"
		},
		{
				.subcmd_id	= CMD_ACK,
				.name		= "Ack",
				.info		= "acks. "
		},
		{
				.subcmd_id	= CMD_WD,
				.name		= "wd",
				.info		= "Suspends watchdog tickle tasks.\n"
								"  reset\n"
								"    Reset but no erase\n"
								"  freset\n"
								"    Reset with erase"
		},
		{
				.subcmd_id	= CMD_SUN,
				.name		= "sun",
				.info		= "Sun sensors.\n"
								"	all\n"
								"		reads sun sensors"
		},
		{
				.subcmd_id	= CMD_DEPLOY,
				.name		= "deploy",
				.info		=  "Deployment disable command."
								"  disarm\n"
								"    Disarm deployment\n"
		},
		{
				.subcmd_id	= CMD_FILE,
				.name		= "file",
				.info		=  "File commands."
								"  dump\n"
								"    Dumps a file.\n"
		},
		{
				.subcmd_id	= CMD_RESTART,
				.name		= "restart",
				.info		=  "Restart commands\n"
		},
		{
				.subcmd_id	= CMD_GPS,
				.name		= "gps",
				.info		=  "GPS get pos [null] and reset\n"
		}
};

int8_t cmdHelp(const CMD_t *cmd) {
	const struct subcmd_opt *subcmd_opt = NULL;
	FOR_EACH(subcmd_opt, CMD_HELP_OPTS) {
		if (cmd->subcmd_id == subcmd_opt->subcmd_id) {
			serialSendQ(subcmd_opt->info);
			return 1;
		}
	}
	if (IS_OUT_OF_BOUNDS(subcmd_opt, CMD_HELP_OPTS)) {
		serialSendQ("help: unknown sub-command");
	}
	return 0;
}


/**
 * Get command.
 */
static const struct subcmd_opt CMD_GET_OPTS[] = {
		{
				.subcmd_id	= CMD_GET_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_GET_TASKS,
				.name		= "tasks",
		},
		{
				.subcmd_id	= CMD_GET_RUNTIME,
				.name		= "runtime",
		},
		{
				.subcmd_id	= CMD_GET_HEAP,
				.name		= "heap",
		},
		{
				.subcmd_id	= CMD_GET_MINHEAP,
				.name		= "minheap",
		},
		{
				.subcmd_id	= CMD_GET_TYPES,
				.name		= "types",
		},
		{
				.subcmd_id	= CMD_GET_EPOCH,
				.name		= "epoch",
		},
};
char buffer[250];
int8_t cmdGet(const CMD_t *cmd) {
	switch (cmd->subcmd_id) {
		case CMD_GET_NONE:
		case CMD_GET_TASKS: {
			serialSendln("TASKS::");
			serialSend("Task\t\tState\tPrio\tStack\tNum\n");
			vTaskList(buffer);
			serialSend(buffer);
			return 1;
		}
		case CMD_GET_RUNTIME: {
			serialSend("Task\t\tAbs\t\tPerc\n");
			vTaskGetRunTimeStats(buffer);
			serialSend(buffer);
			return 1;
		}
		case CMD_GET_HEAP: {
			size_t heapSize = xPortGetFreeHeapSize();
			sprintf(buffer, "%lu bytes\n", heapSize);
			serialSend(buffer);
			return 1;
		}
		case CMD_GET_MINHEAP: {
			size_t heapSize = xPortGetMinimumEverFreeHeapSize();
			sprintf(buffer, "%lu bytes\n", heapSize);
			serialSend(buffer);
			return 1;
		}
		case CMD_GET_TYPES: {
			size_t intSize = sizeof(int);
			size_t intPtrSize = sizeof(int *);
			size_t longSize = sizeof(long);
			size_t longLongSize = sizeof(long long);
			sprintf(buffer, "sizeof(int): %lu bytes\n"
					"sizeof(int *): %lu bytes\n"
					"sizeof(long): %lu bytes\n"
					"sizeof(long long): %lu bytes\n"
					, intSize, intPtrSize, longSize, longLongSize);
			serialSend(buffer);
			return 1;
		}
		case CMD_GET_EPOCH: {
			size_t time = getCurrentTime();
			sprintf(buffer, "Epoch: %i", time);
			serialSend(buffer);
			return 1;
		}
	}
	serialSendQ("get: unknown sub-command");
	return 0;
}

/**
 * Exec command.
 */
static const struct subcmd_opt CMD_EXEC_OPTS[] = {
		{
				.subcmd_id	= CMD_EXEC_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_EXEC_RADIO,
				.name		= "rf",
		},
};
int8_t cmdExec(const CMD_t *cmd) {
	switch (cmd->subcmd_id) {
		case CMD_EXEC_RADIO: {
			initRadio();
			return 1;
		}
	}
	return 0;
}

/*
 * RF Command
 */
static const struct subcmd_opt CMD_RF_OPTS[] = {
		{
				.subcmd_id	= CMD_RF_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_RF_LOOPBACK,
				.name		= "loopback",
		},
		{
				.subcmd_id	= CMD_RF_TX,
				.name		= "tx",
		},
		{
				.subcmd_id	= CMD_RF_RESET,
				.name		= "reset",
		},
		{
				.subcmd_id	= CMD_RF_STX,
				.name		= "stx",
		},

};
int8_t cmdRF(const CMD_t *cmd) {
	switch (cmd->subcmd_id) {
		case CMD_RF_NONE: {
			if(!rfInhibit){
				RadioDAT_t currQueuedPacket = { 0 };
				strcpy((char *)currQueuedPacket.data, "test test test 123");
				currQueuedPacket.size = sizeof("test test test 123") - 1;
				currQueuedPacket.unused = 0xDE;
				xQueueSendToBack(xRadioTXQueue, &currQueuedPacket, 0);

				return 1;
			}
			return 0;
		}
		case CMD_RF_LOOPBACK: {
			if(!rfInhibit){
				uint8_t option = cmd->cmd_data[0];
					switch (option) {
						case 0x00: {
							xTaskNotify(xRadioTaskHandle, 0xBEEFDEAD, eSetValueWithOverwrite);
							serialSendln("RF SPI loopback disabled");
							return 1;
						}
						case 0x10: {
							xTaskNotify(xRadioTaskHandle, 0xDEADBEEF, eSetValueWithOverwrite);
							serialSendln("RF SPI loopback enabled");
							return 1;
						}
					}
					serialSendln("CMD_RF_LOOPBACK unknown selection");
					return 0;
			}
			return 0;
		}
		case CMD_RF_TX:{
			if(!rfInhibit){
				/**
				 * Expect cmd->cmd_data to be a command string set from command system, i.e.:
				 * 		> rf tx get tasks
				 * then cmd->cmd_data will contain bytes "get tasks\0", i.e., it should be a
				 * valid, null-terminated string.
				 *
				 * However, when sending through radio, we don't need these strings to be null-terminated.
				 * Just that they are terminated with \r\n.
				 */
				RadioDAT_t currQueuedPacket = { 0 };
				strcpy((char *)currQueuedPacket.data, (char *)cmd->cmd_data);
				const uint8_t data_len = strlen((char*)cmd->cmd_data);
				currQueuedPacket.data[data_len] = '\r';
				currQueuedPacket.data[data_len + 1] = '\n';
				currQueuedPacket.size = data_len + 2;
				currQueuedPacket.unused = 0xFA;
				xQueueSendToBack(xRadioTXQueue, &currQueuedPacket, 0);
				return 1;
			}
			return 0;
		}
		case CMD_RF_RESET: {
			if(!rfInhibit){
				xTaskNotify(xRadioTaskHandle, RF_NOTIF_RESET, eSetValueWithOverwrite);
				serialSendln("CMD_RF_RESET");
				return 1;
			}
			return 0;
		}
		case CMD_RF_STX: {
			if(!rfInhibit){
				xTaskNotify(xRadioTaskHandle, RF_NOTIF_STX, eSetValueWithOverwrite);
				serialSendln("RF_NOTIF_STX");
				return 1;
			}
			return 0;
		}
	}
	return 0;
}

/**
 * Ack command
 */

int8_t cmdAck(const CMD_t *cmd) {
		if (cmd->subcmd_id == CMD_ACK_NONE){
			serialSendQ("Ack!");
			return 1;
		}
	return 0;
}

/**
 * Watchdog command
 */
static const struct subcmd_opt CMD_WD_OPTS[] = {
		{
				.subcmd_id	= CMD_WD_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_WD_RESET,
				.name		= "reset",
		},
		{
				.subcmd_id	= CMD_WD_F_RESET,
				.name		= "f_reset",
		},
};


int8_t cmdWd(const CMD_t *cmd) {
		if (cmd->subcmd_id == CMD_WD_RESET){
			serialSendQ("Suspending watchdog task!");
			vTaskSuspend(xTickleTaskHandle);
			return 1;
		}
		if (cmd->subcmd_id == CMD_WD_F_RESET){
			serialSendln("Flash erasing");
			flash_erase_chip();
			serialSendln("Flash erased");
			vTaskSuspend(xTickleTaskHandle);
			return 1;
		}

	return 1;
}

/* GPS commands
 *
 */

static const struct subcmd_opt CMD_GPS_OPTS[] = {
		{
				.subcmd_id	= CMD_GPS_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_GPS_RESET,
				.name		= "reset",
		},

};

int8_t cmdGPS(const CMD_t *cmd) {
		if (cmd->subcmd_id == CMD_GPS_RESET){
			restartGPS();
			return 1;
		}
		if (cmd->subcmd_id == CMD_GPS_NONE){
			readGPS();
			return 1;
		}

	return 0;
}

/**
 * Deployables commands
 */
static const struct subcmd_opt CMD_DEPLOY_OPTS[] = {
		{
				.subcmd_id	= CMD_DEPLOY_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_DEPLOY_DISARM,
				.name		= "disarm",
		},
};

int8_t cmdDeploy(const CMD_t *cmd) {
		if (cmd->subcmd_id == CMD_DEPLOY_DISARM){
			deploy_inhibit();
			return 1;
		}
		else{
			return 1;
		}
}

/**
 * File System commands
 */
static const struct subcmd_opt CMD_FILE_OPTS[] = {
		{
				.subcmd_id	= CMD_FILE_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_FILE_DUMP,
				.name		= "dump",
		},
		{
				.subcmd_id	= CMD_FILE_CDUMP,
				.name		= "dumpc",
		},
		{
				.subcmd_id	= CMD_FILE_CPREFIX,
				.name		= "cprefix",
		},
		{
				.subcmd_id	= CMD_FILE_SIZE,
				.name		= "size",
		},
		{
				.subcmd_id	= CMD_FILE_ERASE,
				.name		= "erase",
		},
};

int8_t cmdFile(const CMD_t *cmd) {
		if (cmd->subcmd_id == CMD_FILE_DUMP){
			dumpFile(cmd->cmd_file_data.prefix, cmd->cmd_file_data.suffix);
			return 1;
		}
		if (cmd->subcmd_id == CMD_FILE_CDUMP){
			dumpFile(getCurrentPrefix(), cmd->cmd_file_data.suffix);
			return 1;
		}
		if (cmd->subcmd_id == CMD_FILE_CPREFIX){
			serialSendln((const char*)getCurrentPrefix());
			return 1;
		}
		if (cmd->subcmd_id == CMD_FILE_SIZE){
			// todo: snag this from spiffs struct
			return 1;
		}
		if (cmd->subcmd_id == CMD_FILE_ERASE){
			serialSendln("Flash erasing");
			flash_erase_chip();
			serialSendln("Flash erased");
			fs_num_increments = 0;
			return 1;
		}
		else{
			return 1;
		}
}

/**
 * Sun sensor command
 */
static const struct subcmd_opt CMD_SUN_OPTS[] = {
		{
				.subcmd_id	= CMD_SUN_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_SUN_PLUSX,
				.name		= "x",
		},
		{
				.subcmd_id	= CMD_SUN_MINUSX,
				.name		= "mx",
		},
		{
				.subcmd_id	= CMD_SUN_PLUSY,
				.name		= "y",
		},
		{
				.subcmd_id	= CMD_SUN_MINUSY,
				.name		= "my",
		}
};
int8_t cmdSun(const CMD_t *cmd) {
	switch (cmd->subcmd_id) {
		case CMD_SUN_NONE: {
			serialSendln("SUN:");
//			read_all_mux_channels(0x00);
			read_all_mux_channels(0x4C);
			read_all_mux_channels(0x4D);
			read_all_mux_channels(0x4E);
			read_all_mux_channels(0x4F);
//			read_sun_sensor();					// must loop 4-6 times to get all the values of each sensor of each mux
			return 1;
		} case CMD_SUN_PLUSX: {
			read_all_mux_channels(0x4C);
			return 1;
		} case CMD_SUN_MINUSX: {
			read_all_mux_channels(0x4D);
			return 1;
		} case CMD_SUN_PLUSY: {
			read_all_mux_channels(0x4E);
			return 1;
		} case CMD_SUN_MINUSY: {
			read_all_mux_channels(0x4F);
			return 1;
		}
	}
	return 0;
}

/**
 * Task related commands.
 */
static const struct subcmd_opt CMD_TASK_OPTS[] = {
		{
				.subcmd_id	= CMD_TASK_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_TASK_RESUME,
				.name		= "resume",
		},
		{
				.subcmd_id	= CMD_TASK_SUSPEND,
				.name		= "suspend",
		},
};
int8_t cmdTask(const CMD_t *cmd) {
	switch (cmd->subcmd_id) {
		/**
		 * Show status of all tasks if no arguments are supplied.
		 */
		case CMD_TASK_NONE: {
			return 1;
		}
		/**
		 * Create a task.
		 */
		case CMD_TASK_CREATE: {
			return 1;
		}
		/**
		 * Delete a task.
		 */
		case CMD_TASK_DELETE: {
			switch (cmd->cmd_task_data.task_id) {
				case TASK_SERIAL: {
					vTaskDelete(xSerialTaskHandle);
					return 1;
				} case TASK_RADIO: {
					vTaskDelete(xRadioTaskHandle);
					return 1;
				} case TASK_BLINKY: {
					vTaskDelete(xBlinkyTaskHandle);
					return 1;
				}
			}
			return 0;
		}
		/**
		 * Resume a task.
		 */
		case CMD_TASK_RESUME: {
			switch (cmd->cmd_task_data.task_id) {
				case TASK_SERIAL: {
					vTaskResume(xSerialTaskHandle);
					return 1;
				} case TASK_RADIO: {
					vTaskResume(xRadioTaskHandle);
					return 1;
				} case TASK_BLINKY: {
					vTaskResume(xBlinkyTaskHandle);
					return 1;
				}
			}
			return 0;
		}
		/**
		 * Suspend a task.
		 */
		case CMD_TASK_SUSPEND: {
			switch (cmd->cmd_task_data.task_id) {
				case TASK_SERIAL: {
					vTaskSuspend(xSerialTaskHandle);
					return 1;
				} case TASK_RADIO: {
					vTaskSuspend(xRadioTaskHandle);
					return 1;
				} case TASK_BLINKY: {
					vTaskSuspend(xBlinkyTaskHandle);
					return 1;
				}
			}
			return 0;
		}
		/**
		 * Show runtime status of a task.
		 */
		case CMD_TASK_STATUS: {
			switch (cmd->cmd_task_data.task_id) {
				case TASK_SERIAL: {
					//vTaskGetInfo(xSerialTaskHandle);
					return 1;
				} case TASK_RADIO: {
					//vTaskGetInfo(xRadioTaskHandle);
					return 1;
				} case TASK_BLINKY: {
					//vTaskGetInfo(xBlinkyTaskHandle);
					return 1;
				}
			}
			return 0;
		}
		/**
		 * Show properties of a task.
		 */
		case CMD_TASK_SHOW: {
			return 1;
		}
	}

	return 0;
}

/**
 * Scheduling related commands.
 */
static const struct subcmd_opt CMD_SCHED_OPTS[] = {
		{
				.subcmd_id	= CMD_SCHED_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_SCHED_ADD,
				.name		= "add",
		},
		{
				.subcmd_id	= CMD_SCHED_REMOVE,
				.name		= "remove",
		},
		{
				.subcmd_id	= CMD_SCHED_SHOW,
				.name		= "show",
		},
};
int8_t cmdSched(const CMD_t *cmd) {
	switch (cmd->subcmd_id) {
		case CMD_SCHED_NONE: {
			return 1;
		}
		case CMD_SCHED_ADD: {
			CMD_t event_cmd = {
					.cmd_id = cmd->cmd_sched_data.scheduled_cmd_id,
					.subcmd_id = cmd->cmd_sched_data.scheduled_subcmd_id,
			};
			memcpy(event_cmd.cmd_data, cmd->cmd_sched_data.scheduled_cmd_data, CMD_DATA_MAX_SIZE);
			Event_t event = {
					.seconds_from_now = cmd->cmd_sched_data.seconds_from_now,
					.action = event_cmd,
			};
			addEvent(event);
			return 1;
		}
		case CMD_SCHED_REMOVE: {
			const int idx = cmd->cmd_sched_data.cmd_sched_misc_data.event_idx_to_remove;
			return removeEventIdx(idx);
		}
		case CMD_SCHED_SHOW: {
			serialSendQ("schedule:\n");
			showActiveEvents();
			return 1;
		}
	}

	return 0;
}

/**
 * State related commands.
 */
static const struct subcmd_opt CMD_STATE_OPTS[] = {
		{
				.subcmd_id	= CMD_STATE_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_STATE_GET,
				.name		= "get",
		},
		{
				.subcmd_id	= CMD_STATE_SET,
				.name		= "set",
		},
		{
				.subcmd_id	= CMD_STATE_PREV,
				.name		= "prev",
		},
		{
				.subcmd_id	= CMD_STATE_ENTRY,
				.name		= "entry",
		},
};
int8_t cmdState(const CMD_t *cmd) {
	switch (cmd->subcmd_id) {
		/**
		 * Fall through (do a CMD_STATE_GET)
		 */
		case CMD_STATE_NONE: {
		}
		/**
		 * Get current state
		 */
		case CMD_STATE_GET: {
			printStateInfo(cur_state,&state_persistent_data);
			return 1;
		}
		/**
		 * Set system state (can be scheduled in the future too)
		 */
		case CMD_STATE_SET: {
			switch (cmd->cmd_state_data.state_id) {
				case STATE_SAFE: {
					if(STATE_SAFE == setStateManual(&state_persistent_data, STATE_SAFE)){
						serialSendQ("SET STATE SAFE");
						return 1;
					}

				} case STATE_READY: {
					if(STATE_READY == setStateManual(&state_persistent_data, STATE_READY)){
						serialSendQ("SET STATE READY");
						return 1;
					}
				} case STATE_LOW_POWER: {
					if(STATE_LOW_POWER == setStateManual(&state_persistent_data, STATE_LOW_POWER)){
						serialSendQ("SET STATE LOW POWER");
						return 1;
					}
				}
			}
			return 0;
		}
		/**
		 * Get previous state
		 */
		case CMD_STATE_PREV: {
			printPrevState(cur_state,&state_persistent_data);
			return 1;
		}
		/*
		 * Return the time we entered the current state
		 */
		case CMD_STATE_ENTRY: {
			printStateEntryTime();
			return 1;
		}
	}

	return 0;
}


/**
 * System restart commands
 */
static const struct subcmd_opt CMD_RESTART_OPTS[] = {
		{
				.subcmd_id	= CMD_RESTART_NONE,
				.name		= "",
		},
		{
				.subcmd_id	= CMD_RESTART_ERASE_FILES,
				.name		= "erase",
		},
};

int8_t cmdRestart(const CMD_t *cmd) {

	/* NOTE:
	 * 	THESE CAUSE A DATA ABORT
	 * 		- that's why the call is commented out
	 */

		if (cmd->subcmd_id == CMD_RESTART_NONE){
//			restart_software();
			return 1;
		}
		if (cmd->subcmd_id == CMD_RESTART_ERASE_FILES){
//			flash_erase_chip();
//			restart_software();
			return 1;
		}
		else{
			return 1;
		}
}

/**
 * Command table.
 *
 * If adding a new entry, make sure all fields are initialized:
 * 		- .name is required to be set to something
 */
static const struct cmd_opt CMD_OPTS[] = {
		{
				.cmd_id			= CMD_HELP,
				.func			= cmdHelp,
				.name			= "help",
				.subcmds		= CMD_HELP_OPTS,
				.num_subcmds 	= LEN(CMD_HELP_OPTS),
		},
		{
				.cmd_id			= CMD_GET,
				.func			= cmdGet,
				.name			= "get",
				.subcmds		= CMD_GET_OPTS,
				.num_subcmds	= LEN(CMD_GET_OPTS),
		},
		{
				.cmd_id			= CMD_EXEC,
				.func			= cmdExec,
				.name			= "exec",
				.subcmds		= CMD_EXEC_OPTS,
				.num_subcmds	= LEN(CMD_EXEC_OPTS),
		},
		{
				.cmd_id			= CMD_RF,
				.func			= cmdRF,
				.name			= "rf",
				.subcmds		= CMD_RF_OPTS,
				.num_subcmds	= LEN(CMD_RF_OPTS),
		},
		{
				.cmd_id			= CMD_TASK,
				.func			= cmdTask,
				.name			= "task",
				.subcmds		= CMD_TASK_OPTS,
				.num_subcmds	= LEN(CMD_TASK_OPTS),
		},
		{
				.cmd_id			= CMD_SCHED,
				.name			= "sched",
				.func			= cmdSched,
				.subcmds		= CMD_SCHED_OPTS,
				.num_subcmds	= LEN(CMD_SCHED_OPTS),
		},
		{
				.cmd_id			= CMD_STATE,
				.name			= "state",
				.func			= cmdState,
				.subcmds		= CMD_STATE_OPTS,
				.num_subcmds	= LEN(CMD_STATE_OPTS),
		},
		{
				.cmd_id			= CMD_ACK,
				.name			= "ack",
				.func			= cmdAck,
				.subcmds		= NULL,
				.num_subcmds	= 0,
		},
		{
				.cmd_id			= CMD_WD,
				.name			= "wd",
				.func			= cmdWd,
				.subcmds		= CMD_WD_OPTS,
				.num_subcmds	= LEN(CMD_WD_OPTS),
		},
		{
				.cmd_id			= CMD_SUN,
				.name			= "sun",
				.func			= cmdSun,
				.subcmds		= CMD_SUN_OPTS,
				.num_subcmds	= LEN(CMD_SUN_OPTS),
		},
		{
      	  	  	.cmd_id			= CMD_DEPLOY,
				.name			= "deploy",
				.func			= cmdDeploy,
				.subcmds		= CMD_DEPLOY_OPTS,
				.num_subcmds	= LEN(CMD_DEPLOY_OPTS),
		},
		{
				.cmd_id			= CMD_FILE,
				.name			= "file",
				.func			= cmdFile,
				.subcmds		= CMD_FILE_OPTS,
				.num_subcmds	= LEN(CMD_FILE_OPTS),
		},
		{
				.cmd_id			= CMD_RESTART,
				.name			= "restart",
				.func			= cmdRestart,
				.subcmds		= CMD_RESTART_OPTS,
				.num_subcmds	= LEN(CMD_RESTART_OPTS),
		},
		{
				.cmd_id			= CMD_GPS,
				.name			= "gps",
				.func			= cmdGPS,
				.subcmds		= CMD_GPS_OPTS,
				.num_subcmds	= LEN(CMD_GPS_OPTS),
		}
};

/**
 * Quickly get size of arbitrary structures using map file and grep.
 *
 * For example...
 * 		- `$ grep .data:test Debug/OBC.map`
 * 		   > 080056c8    00000018     sfu_cmds.obj (.data:testCMD)
 * 		   > 080056f4    00000014     sfu_cmds.obj (.data:testSCHED)
 */
#ifdef _DEBUG
CMD_t testCMD = {0};
CMD_SCHED_DATA_t testSCHED = {0};
void __unused() {
	testCMD.cmd_id = CMD_GET;
	testSCHED.scheduled_cmd_id = CMD_GET;
}
#endif

int8_t checkAndRunCommand(const CMD_t *cmd) {
	const struct cmd_opt *cmd_opt = NULL;
	FOR_EACH(cmd_opt, CMD_OPTS) {
		if (cmd_opt->cmd_id == cmd->cmd_id) {
			cmd_opt->func(cmd);
			return 1;
		}
	}
	return 0;
}

int8_t checkAndRunCommandStr(char *cmd) {
	size_t running_total = 0;
	char cmd_copy[128] = {0};
	strcpy(cmd_copy, cmd);

	const char delim[] = " ";
	char *intendedCmd = strtok(cmd, delim);
	/**
	 * Exit if we could not get the first token.
	 */
	if (intendedCmd == NULL) return 0;

	/**
	 * Compare the first word (which is the user's intended command) with all known
	 * commands. The iterator cmd_opt is set on each iteration.
	 */
	const struct cmd_opt *cmd_opt;
	FOR_EACH(cmd_opt, CMD_OPTS) {
		if(strcmp(intendedCmd, cmd_opt->name) == 0) {
			break;
		}
	}
	/**
	 * Exit if the command does not exist.
	 */
	if (IS_OUT_OF_BOUNDS(cmd_opt, CMD_OPTS)) return 0;

	running_total += strlen(intendedCmd) + 1; // + len of delim

	/**
	 * Compare the second word if it exists (which is the user's intended sub-command)
	 * with all known sub-commands for that command.
	 *
	 * Save the sub-command ID if a match is found, otherwise subcmd_id will be set to
	 * CMD_UNDEFINED (for an unrecognized sub-command) or CMD_DEFAULT (if no sub-command given).
	 */
	intendedCmd = strtok(NULL, delim);
	const struct subcmd_opt *subcmd_opt = NULL;
	uint8_t subcmd_id = intendedCmd ? CMD_UNDEFINED : CMD_DEFAULT;
	for (subcmd_opt = cmd_opt->subcmds; subcmd_opt != NULL &&
										intendedCmd != NULL &&
										subcmd_opt < &cmd_opt->subcmds[cmd_opt->num_subcmds]; subcmd_opt++)
	{
		if(strcmp(intendedCmd, subcmd_opt->name) == 0) {
			subcmd_id = subcmd_opt->subcmd_id;
			running_total += strlen(intendedCmd) + 1; // + len of delim
			break;
		}
	}

	CMD_t cmd_t = {.cmd_id = cmd_opt->cmd_id, .subcmd_id = subcmd_id};

	/**
	 * The third token will be interpreted as a hex string if it exists.
	 * No preceding 0x required.
	 * The command's argument field, cmd_data, will be set to this hex data.
	 *
	 * Data is parsed in pairs of hex characters.
	 *
	 * Example:
	 * 		> task suspend 40
	 *
	 * 		Data will be == "40\0"
	 * 		This will be parsed into the corresponding byte, and then
	 * 		cmd_t.cmd_task_data.task_id will be == TASK_BLINKY (4)
	 */
	const char *data = strtok(NULL, delim);
	unsigned int i = 0;
	if (data != NULL) {
		const int data_len = strlen(data);
		if (cmd_t.cmd_id == CMD_RF && cmd_t.subcmd_id == CMD_RF_TX) {
			strncpy((char *)cmd_t.cmd_data, cmd_copy + running_total, sizeof(cmd_t.cmd_data) - 1);
			cmd_t.cmd_data[sizeof(cmd_t.cmd_data) - 1] = '\0';
			if (strlen(cmd_copy) > sizeof(cmd_t.cmd_data)) {
				serialSend("WARNING: string to TX too large; truncated to ");
				sprintf(buffer, "%d bytes!", sizeof(cmd_t.cmd_data) - 1);
				serialSendln(buffer);
			}
			serialSend("\tcmd_data[]=\"");
			serialSend((char *)cmd_t.cmd_data);
			serialSend("\"");
		} else {
			for (i = 0; i < CMD_DATA_MAX_SIZE * 2 && i < data_len; i += 2) {
				char c[3] = {NULL};
				c[0] = *(data + i);
				// TODO: fix dereference beyond null char
				const char n = *(data + i + 1);
				c[1] = n == '\0' ? '0' : n;
				cmd_t.cmd_data[i / 2] = strtol(c, NULL, 16);
			}
			serialSend((char *)cmd_t.cmd_data);
		}
	}


	/**
	 * Invoke the intended command with the command struct created above.
	 */
	cmd_opt->func(&cmd_t);

	return 1;
}
