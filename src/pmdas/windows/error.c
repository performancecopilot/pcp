/*
 * Error code -> message map comes from
 * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/perfmon/base/pdh_error_codes.asp
 *
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "hypnotoad.h"

static struct {
    int		code;
    char	*msg;
} errtab[] = {
    { PDH_CSTATUS_VALID_DATA, "The returned data is valid." },
    { PDH_CSTATUS_NEW_DATA, "The return data value is valid and different from the last sample." },
    { PDH_CSTATUS_NO_MACHINE, "Unable to connect to specified machine or machine is off line." },
    { PDH_CSTATUS_NO_INSTANCE, "The specified instance is not present." },
    { PDH_MORE_DATA, "There is more data to return than would fit in the supplied buffer. Allocate a larger buffer and call the function again." },
    { PDH_CSTATUS_ITEM_NOT_VALIDATED, "The data item has been added to the query but has not been validated nor accessed. No other status information on this data item is available." },
    { PDH_RETRY, "The selected operation should be retried." },
    { PDH_NO_DATA, "No data to return." },
    { PDH_CALC_NEGATIVE_DENOMINATOR, "A counter with a negative denominator value was detected." },
    { PDH_CALC_NEGATIVE_TIMEBASE, "A counter with a negative time base value was detected." },
    { PDH_CALC_NEGATIVE_VALUE, "A counter with a negative value was detected." },
    { PDH_DIALOG_CANCELLED, "The user canceled the dialog box." },
    { PDH_END_OF_LOG_FILE, "The end of the log file was reached." },
    { PDH_ASYNC_QUERY_TIMEOUT, "Time out while waiting for asynchronous counter collection thread to end." },
    { PDH_CANNOT_SET_DEFAULT_REALTIME_DATASOURCE, "Cannot change set default real-time data source. There are real-time query sessions collecting counter data." },
    { PDH_CSTATUS_NO_OBJECT, "The specified object is not found on the system." },
    { PDH_CSTATUS_NO_COUNTER, "The specified counter could not be found." },
    { PDH_CSTATUS_INVALID_DATA, "The returned data is not valid." },
    { PDH_MEMORY_ALLOCATION_FAILURE, "A PDH function could not allocate enough temporary memory to complete the operation. Close some applications or extend the page file and retry the function." },
    { PDH_INVALID_HANDLE, "The handle is not a valid PDH object." },
    { PDH_INVALID_ARGUMENT, "A required argument is missing or incorrect." },
    { PDH_FUNCTION_NOT_FOUND, "Unable to find the specified function." },
    { PDH_CSTATUS_NO_COUNTERNAME, "No counter was specified." },
    { PDH_CSTATUS_BAD_COUNTERNAME, "Unable to parse the counter path. Check the format and syntax of the specified path." },
    { PDH_INVALID_BUFFER, "The buffer passed by the caller is invalid." },
    { PDH_INSUFFICIENT_BUFFER, "The requested data is larger than the buffer supplied. Unable to return the requested data." },
    { PDH_CANNOT_CONNECT_MACHINE, "Unable to connect to the requested machine." },
    { PDH_INVALID_PATH, "The specified counter path could not be interpreted." },
    { PDH_INVALID_INSTANCE, "The instance name could not be read from the specified counter path." },
    { PDH_INVALID_DATA, "The data is not valid." },
    { PDH_NO_DIALOG_DATA, "The dialog box data block was missing or invalid." },
    { PDH_CANNOT_READ_NAME_STRINGS, "Unable to read the counter and/or explain text from the specified machine." },
    { PDH_LOG_FILE_CREATE_ERROR, "Unable to create the specified log file." },
    { PDH_LOG_FILE_OPEN_ERROR, "Unable to open the specified log file." },
    { PDH_LOG_TYPE_NOT_FOUND, "The specified log file type has not been installed on this system." },
    { PDH_NO_MORE_DATA, "No more data is available." },
    { PDH_ENTRY_NOT_IN_LOG_FILE, "The specified record was not found in the log file." },
    { PDH_DATA_SOURCE_IS_LOG_FILE, "The specified data source is a log file." },
    { PDH_DATA_SOURCE_IS_REAL_TIME, "The specified data source is the current activity." },
    { PDH_UNABLE_READ_LOG_HEADER, "The log file header could not be read." },
    { PDH_FILE_NOT_FOUND, "Unable to find the specified file." },
    { PDH_FILE_ALREADY_EXISTS, "There is already a file with the specified file name." },
    { PDH_NOT_IMPLEMENTED, "The function referenced has not been implemented." },
    { PDH_STRING_NOT_FOUND, "Unable to find the specified string in the list of performance name and explain text strings." },
    { PDH_UNABLE_MAP_NAME_FILES, "Unable to map to the performance counter name data files. The data will be read from the registry and stored locally." },
    { PDH_UNKNOWN_LOG_FORMAT, "The format of the specified log file is not recognized by the PDH DLL." },
    { PDH_UNKNOWN_LOGSVC_COMMAND, "The specified Log Service command value is not recognized." },
    { PDH_LOGSVC_QUERY_NOT_FOUND, "The specified Query from the Log Service could not be found or could not be opened." },
    { PDH_LOGSVC_NOT_OPENED, "The Performance Data Log Service key could not be opened. This may be due to insufficient privilege or because the service has not been installed." },
    { PDH_WBEM_ERROR, "An error occurred while accessing the WBEM data store." },
    { PDH_ACCESS_DENIED, "Unable to access the desired machine or service. Check the permissions and authentication of the log service or the interactive user session against those on the machine or service being monitored." },
    { PDH_LOG_FILE_TOO_SMALL, "The maximum log file size specified is too small to log the selected counters. No data will be recorded in this log file. Specify a smaller set of counters to log or a larger file size and retry this call." },
    { PDH_INVALID_DATASOURCE, "Cannot connect to ODBC DataSource Name." },
    { PDH_INVALID_SQLDB, "SQL Database does not contain a valid set of tables for Perfmon; use PdhCreateSQLTables." },
    { PDH_NO_COUNTERS, "No counters were found for this Perfmon SQL Log Set." },
    { PDH_SQL_ALLOC_FAILED, "Call to SQLAllocStmt failed with %1." },
    { PDH_SQL_ALLOCCON_FAILED, "Call to SQLAllocConnect failed with %1." },
    { PDH_SQL_EXEC_DIRECT_FAILED, "Call to SQLExecDirect failed with %1." },
    { PDH_SQL_FETCH_FAILED, "Call to SQLFetch failed with %1." },
    { PDH_SQL_ROWCOUNT_FAILED, "Call to SQLRowCount failed with %1." },
    { PDH_SQL_MORE_RESULTS_FAILED, "Call to SQLMoreResults failed with %1." },
    { PDH_SQL_CONNECT_FAILED, "Call to SQLConnect failed with %1." },
    { PDH_SQL_BIND_FAILED, "Call to SQLBindCol failed with %1." },
    { PDH_CANNOT_CONNECT_WMI_SERVER, "Unable to connect to the WMI server on requested machine." },
    { PDH_PLA_COLLECTION_ALREADY_RUNNING, "Collection %1s is already running." },
    { PDH_PLA_ERROR_SCHEDULE_OVERLAP, "The specified start time is after the end time." },
    { PDH_PLA_COLLECTION_NOT_FOUND, "Collection %1 does not exist." },
    { PDH_PLA_ERROR_SCHEDULE_ELAPSED, "The specified end time has already elapsed." },
    { PDH_PLA_ERROR_NOSTART, "Collection %1 did not start check the application event log for any errors." },
    { PDH_PLA_ERROR_ALREADY_EXISTS, "Collection %1 already exists." },
    { PDH_PLA_ERROR_TYPE_MISMATCH, "There is a mismatch in the settings type." },
    { PDH_PLA_ERROR_FILEPATH, "The information specified does not resolve to a valid path name." },
    { PDH_PLA_SERVICE_ERROR, "The 'Performance Logs & Alerts' service did not respond." },
    { PDH_PLA_VALIDATION_ERROR, "The information passed is not valid." },
    { PDH_PLA_VALIDATION_WARNING, "The information passed is not valid." },
    { PDH_PLA_ERROR_NAME_TOO_LONG, "The name supplied is too long." },
    { PDH_INVALID_SQL_LOG_FORMAT, "SQL log format is incorrect. Correct format is 'SQL:<DSN-name>!<LogSet-Name>'." },
    { PDH_COUNTER_ALREADY_IN_QUERY, "Performance counter in PdhAddCounter call has already been added in the performance query. This counter is ignored." },
    { PDH_BINARY_LOG_CORRUPT, "Unable to read counter information and data from input binary log files." },
    { PDH_LOG_SAMPLE_TOO_SMALL, "At least one of the input binary log files contain fewer than two data samples." },
    { PDH_OS_LATER_VERSION, "The version of the operating system on the computer named %1 is later than that on the local computer. This operation is not available from the local computer." },
    { PDH_OS_EARLIER_VERSION, "supports %2 or later. Check the operating system version on the computer named %3." },
    { PDH_INCORRECT_APPEND_TIME, "The output file must contain earlier data than the file to be appended." },
    { PDH_UNMATCHED_APPEND_COUNTER, "Both files must have identical counters in order to append." },
    { PDH_SQL_ALTER_DETAIL_FAILED, "Cannot alter CounterDetail table layout in SQL database." },
    { PDH_QUERY_PERF_DATA_TIMEOUT, "System is busy. Timeout when collecting counter data. Please retry later or increase the CollectTime registry value." }
};

static int sz_errtab = sizeof(errtab) / sizeof(errtab[0]);
static char *buf = "eh? 0x........";

char *pdherrstr(int code)
{
    int i;

    for (i = 0; i < sz_errtab; i++) {
	if (code == errtab[i].code)
	    return errtab[i].msg;
    }
    sprintf(buf, "eh? 0x%08x", code);
    return buf;
}
