#ifndef WINEVT_H
#define WINEVT_H

typedef HANDLE EVT_HANDLE, *PEVT_HANDLE;

typedef enum _EVT_VARIANT_TYPE
{
    EvtVarTypeNull        = 0,
    EvtVarTypeString      = 1,
    EvtVarTypeAnsiString  = 2,
    EvtVarTypeSByte       = 3,
    EvtVarTypeByte        = 4,
    EvtVarTypeInt16       = 5,
    EvtVarTypeUInt16      = 6,
    EvtVarTypeInt32       = 7,
    EvtVarTypeUInt32      = 8,
    EvtVarTypeInt64       = 9,
    EvtVarTypeUInt64      = 10,
    EvtVarTypeSingle      = 11,
    EvtVarTypeDouble      = 12,
    EvtVarTypeBoolean     = 13,
    EvtVarTypeBinary      = 14,
    EvtVarTypeGuid        = 15,
    EvtVarTypeSizeT       = 16,
    EvtVarTypeFileTime    = 17,
    EvtVarTypeSysTime     = 18,
    EvtVarTypeSid         = 19,
    EvtVarTypeHexInt32    = 20,
    EvtVarTypeHexInt64    = 21,

    // these types used internally
    EvtVarTypeEvtHandle   = 32,
    EvtVarTypeEvtXml      = 35

} EVT_VARIANT_TYPE;


#define EVT_VARIANT_TYPE_MASK 0x7f
#define EVT_VARIANT_TYPE_ARRAY 128


typedef struct _EVT_VARIANT
{
    union
    {
        BOOL        BooleanVal;
        INT8        SByteVal;
        INT16       Int16Val;
        INT32       Int32Val;
        INT64       Int64Val;
        UINT8       ByteVal;
        UINT16      UInt16Val;
        UINT32      UInt32Val;
        UINT64      UInt64Val;
        float       SingleVal;
        double      DoubleVal;
        ULONGLONG   FileTimeVal;
        SYSTEMTIME* SysTimeVal;
        GUID*       GuidVal;
        LPCWSTR     StringVal;
        LPCSTR      AnsiStringVal;
        PBYTE       BinaryVal;
        PSID        SidVal;
        size_t      SizeTVal;

        // array fields
        BOOL*       BooleanArr;
        INT8*       SByteArr;
        INT16*      Int16Arr;
        INT32*      Int32Arr;
        INT64*      Int64Arr;
        UINT8*      ByteArr;
        UINT16*     UInt16Arr;
        UINT32*     UInt32Arr;
        UINT64*     UInt64Arr;
        float*      SingleArr;
        double*     DoubleArr;
        FILETIME*   FileTimeArr;
        SYSTEMTIME* SysTimeArr;
        GUID*       GuidArr;
        LPWSTR*     StringArr;
        LPSTR*      AnsiStringArr;
        PSID*       SidArr;
        size_t*     SizeTArr;

        // internal fields
        EVT_HANDLE  EvtHandleVal;
        LPCWSTR     XmlVal;
        LPCWSTR*    XmlValArr;
    };

    DWORD Count;   // number of elements (not length) in bytes.
    DWORD Type;

} EVT_VARIANT, *PEVT_VARIANT;

#if 0

////////////////////////////////////////////////////////////////////////////////
//
// Sessions
//
////////////////////////////////////////////////////////////////////////////////

typedef enum _EVT_LOGIN_CLASS
{
    EvtRpcLogin = 1

} EVT_LOGIN_CLASS;

typedef enum _EVT_RPC_LOGIN_FLAGS
{
    EvtRpcLoginAuthDefault = 0,
    EvtRpcLoginAuthNegotiate,
    EvtRpcLoginAuthKerberos,
    EvtRpcLoginAuthNTLM

} EVT_RPC_LOGIN_FLAGS;

typedef struct _EVT_RPC_LOGIN
{
    // all str params are optional
    LPWSTR  Server;
    LPWSTR  User;
    LPWSTR  Domain;
    LPWSTR  Password;
    DWORD   Flags;                      // EVT_RPC_LOGIN_FLAGS

} EVT_RPC_LOGIN;

EVT_HANDLE WINAPI EvtOpenSession(
    EVT_LOGIN_CLASS LoginClass,
    PVOID Login,
    __reserved DWORD Timeout,           // currently must be 0
    __reserved DWORD Flags              // currently must be 0
    );

#endif

////////////////////////////////////////////////////////////////////////////////
//
// General Purpose Functions
//
////////////////////////////////////////////////////////////////////////////////


BOOL WINAPI EvtClose(
    EVT_HANDLE Object
    );

#if 0

BOOL WINAPI EvtCancel(
    EVT_HANDLE Object
    );

DWORD WINAPI EvtGetExtendedStatus(
    DWORD BufferSize,
    __out_ecount_part_opt(BufferSize, *BufferUsed) LPWSTR Buffer,
    __out PDWORD BufferUsed
    );

#endif

////////////////////////////////////////////////////////////////////////////////
//
// Queries
//
////////////////////////////////////////////////////////////////////////////////


typedef enum _EVT_QUERY_FLAGS
{
    EvtQueryChannelPath                 = 0x1,
    EvtQueryFilePath                    = 0x2,

    EvtQueryForwardDirection            = 0x100,
    EvtQueryReverseDirection            = 0x200,

    EvtQueryTolerateQueryErrors         = 0x1000

} EVT_QUERY_FLAGS;

EVT_HANDLE WINAPI EvtQuery(
    EVT_HANDLE Session,
    LPCWSTR Path,
    LPCWSTR Query,
    DWORD Flags
    );

BOOL WINAPI EvtNext(
    EVT_HANDLE ResultSet,
    DWORD EventsSize,
    PEVT_HANDLE Events,
    DWORD Timeout,
    DWORD Flags,
    PDWORD Returned
    );

#if 0

typedef enum _EVT_SEEK_FLAGS
{
    EvtSeekRelativeToFirst    = 1,
    EvtSeekRelativeToLast     = 2,
    EvtSeekRelativeToCurrent  = 3,
    EvtSeekRelativeToBookmark = 4,
    EvtSeekOriginMask         = 7,

    EvtSeekStrict             = 0x10000,

} EVT_SEEK_FLAGS;



BOOL WINAPI EvtSeek(
    EVT_HANDLE ResultSet,
    LONGLONG Position,
    EVT_HANDLE Bookmark,
    __reserved DWORD Timeout,           // currently must be 0
    DWORD Flags
    );


////////////////////////////////////////////////////////////////////////////////
//
// Subscriptions
//
////////////////////////////////////////////////////////////////////////////////

typedef enum _EVT_SUBSCRIBE_FLAGS
{
    EvtSubscribeToFutureEvents      = 1,
    EvtSubscribeStartAtOldestRecord = 2,
    EvtSubscribeStartAfterBookmark  = 3,
    EvtSubscribeOriginMask          = 3,

    EvtSubscribeTolerateQueryErrors = 0x1000,

    EvtSubscribeStrict              = 0x10000,

} EVT_SUBSCRIBE_FLAGS;

typedef enum _EVT_SUBSCRIBE_NOTIFY_ACTION
{
    EvtSubscribeActionError = 0,
    EvtSubscribeActionDeliver

} EVT_SUBSCRIBE_NOTIFY_ACTION;

typedef DWORD (WINAPI *EVT_SUBSCRIBE_CALLBACK)(
    EVT_SUBSCRIBE_NOTIFY_ACTION Action,
    PVOID UserContext,
    EVT_HANDLE Event );

EVT_HANDLE WINAPI EvtSubscribe(
    EVT_HANDLE Session,
    HANDLE SignalEvent,
    LPCWSTR ChannelPath,
    LPCWSTR Query,
    EVT_HANDLE Bookmark,
    PVOID context,
    EVT_SUBSCRIBE_CALLBACK Callback,
    DWORD Flags
    );

#endif

////////////////////////////////////////////////////////////////////////////////
//
// Rendering
//
////////////////////////////////////////////////////////////////////////////////

typedef enum _EVT_SYSTEM_PROPERTY_ID
{
    EvtSystemProviderName = 0,          // EvtVarTypeString             
    EvtSystemProviderGuid,              // EvtVarTypeGuid  
    EvtSystemEventID,                   // EvtVarTypeUInt16  
    EvtSystemQualifiers,                // EvtVarTypeUInt16
    EvtSystemLevel,                     // EvtVarTypeUInt8
    EvtSystemTask,                      // EvtVarTypeUInt16
    EvtSystemOpcode,                    // EvtVarTypeUInt8
    EvtSystemKeywords,                  // EvtVarTypeHexInt64
    EvtSystemTimeCreated,               // EvtVarTypeFileTime
    EvtSystemEventRecordId,             // EvtVarTypeUInt64
    EvtSystemActivityID,                // EvtVarTypeGuid
    EvtSystemRelatedActivityID,         // EvtVarTypeGuid
    EvtSystemProcessID,                 // EvtVarTypeUInt32
    EvtSystemThreadID,                  // EvtVarTypeUInt32
    EvtSystemChannel,                   // EvtVarTypeString 
    EvtSystemComputer,                  // EvtVarTypeString 
    EvtSystemUserID,                    // EvtVarTypeSid
    EvtSystemVersion,                   // EvtVarTypeUInt8
    EvtSystemPropertyIdEND

} EVT_SYSTEM_PROPERTY_ID;

typedef enum _EVT_RENDER_CONTEXT_FLAGS
{
    EvtRenderContextValues = 0,         // Render specific properties
    EvtRenderContextSystem,             // Render all system properties (System)
    EvtRenderContextUser                // Render all user properties (User/EventData)
} EVT_RENDER_CONTEXT_FLAGS;

typedef enum _EVT_RENDER_FLAGS
{
    EvtRenderEventValues = 0,           // Variants
    EvtRenderEventXml,                  // XML
    EvtRenderBookmark                   // Bookmark
} EVT_RENDER_FLAGS;

EVT_HANDLE WINAPI EvtCreateRenderContext(
    DWORD ValuePathsCount,
    LPCWSTR* ValuePaths,
    DWORD Flags                         // EVT_RENDER_CONTEXT_FLAGS
    );

BOOL WINAPI EvtRender(
    EVT_HANDLE Context,
    EVT_HANDLE Fragment,
    DWORD Flags,                        // EVT_RENDER_FLAGS
    DWORD BufferSize,
    PVOID Buffer,
    PDWORD BufferUsed,
    PDWORD PropertyCount
    );

typedef enum _EVT_FORMAT_MESSAGE_FLAGS
{
    EvtFormatMessageEvent = 1,
    EvtFormatMessageLevel,
    EvtFormatMessageTask,
    EvtFormatMessageOpcode,
    EvtFormatMessageKeyword,
    EvtFormatMessageChannel, 
    EvtFormatMessageProvider, 
    EvtFormatMessageId,
    EvtFormatMessageXml,

} EVT_FORMAT_MESSAGE_FLAGS;

BOOL WINAPI EvtFormatMessage(
    EVT_HANDLE PublisherMetadata,       // Except for forwarded events
    EVT_HANDLE Event,
    DWORD MessageId,
    DWORD ValueCount,
    PEVT_VARIANT Values,
    DWORD Flags,
    DWORD BufferSize,
    LPWSTR Buffer,
    PDWORD BufferUsed
    );


#if 0

////////////////////////////////////////////////////////////////////////////////
//
// Log Maintenace and Information
//
////////////////////////////////////////////////////////////////////////////////

typedef enum _EVT_OPEN_LOG_FLAGS
{
    EvtOpenChannelPath          = 0x1,
    EvtOpenFilePath             = 0x2

} EVT_OPEN_LOG_FLAGS;

typedef enum _EVT_LOG_PROPERTY_ID
{
    EvtLogCreationTime = 0,             // EvtVarTypeFileTime
    EvtLogLastAccessTime,               // EvtVarTypeFileTime
    EvtLogLastWriteTime,                // EvtVarTypeFileTime
    EvtLogFileSize,                     // EvtVarTypeUInt64
    EvtLogAttributes,                   // EvtVarTypeUInt32
    EvtLogNumberOfLogRecords,           // EvtVarTypeUInt64
    EvtLogOldestRecordNumber,           // EvtVarTypeUInt64
    EvtLogFull,                         // EvtVarTypeBoolean

} EVT_LOG_PROPERTY_ID;

EVT_HANDLE WINAPI EvtOpenLog(
    EVT_HANDLE Session,
    LPCWSTR Path,
    DWORD Flags
    );

BOOL WINAPI EvtGetLogInfo(
    EVT_HANDLE Log,
    EVT_LOG_PROPERTY_ID PropertyId,
    DWORD PropertyValueBufferSize,
    PEVT_VARIANT PropertyValueBuffer,
    __out PDWORD PropertyValueBufferUsed
    );

BOOL WINAPI EvtClearLog(
    EVT_HANDLE Session,
    LPCWSTR ChannelPath,
    LPCWSTR TargetFilePath,
    DWORD Flags
    );

typedef enum _EVT_EXPORTLOG_FLAGS
{
    EvtExportLogChannelPath     = 0x1,
    EvtExportLogFilePath        = 0x2,
    EvtExportLogTolerateQueryErrors = 0x1000

} EVT_EXPORTLOG_FLAGS;

BOOL WINAPI EvtExportLog(
    EVT_HANDLE Session,
    LPCWSTR Path,
    LPCWSTR Query,
    LPCWSTR TargetFilePath,
    DWORD Flags
    );

BOOL WINAPI EvtArchiveExportedLog(
    EVT_HANDLE Session,
    LPCWSTR LogFilePath,
    LCID Locale,
    DWORD Flags
    );

////////////////////////////////////////////////////////////////////////////////
//
// Channel Configuration
//
////////////////////////////////////////////////////////////////////////////////


typedef enum _EVT_CHANNEL_CONFIG_PROPERTY_ID
{
    EvtChannelConfigEnabled = 0,            // EvtVarTypeBoolean
    EvtChannelConfigIsolation,              // EvtVarTypeUInt32, EVT_CHANNEL_ISOLATION_TYPE
    EvtChannelConfigType,                   // EvtVarTypeUInt32, EVT_CHANNEL_TYPE
    EvtChannelConfigOwningPublisher,        // EvtVarTypeString
    EvtChannelConfigClassicEventlog,        // EvtVarTypeBoolean
    EvtChannelConfigAccess,                 // EvtVarTypeString
    EvtChannelLoggingConfigRetention,       // EvtVarTypeBoolean
    EvtChannelLoggingConfigAutoBackup,      // EvtVarTypeBoolean
    EvtChannelLoggingConfigMaxSize,         // EvtVarTypeUInt64
    EvtChannelLoggingConfigLogFilePath,     // EvtVarTypeString
    EvtChannelPublishingConfigLevel,        // EvtVarTypeUInt32
    EvtChannelPublishingConfigKeywords,     // EvtVarTypeUInt64
    EvtChannelPublishingConfigControlGuid,  // EvtVarTypeGuid
    EvtChannelPublishingConfigBufferSize,   // EvtVarTypeUInt32
    EvtChannelPublishingConfigMinBuffers,   // EvtVarTypeUInt32
    EvtChannelPublishingConfigMaxBuffers,   // EvtVarTypeUInt32
    EvtChannelPublishingConfigLatency,      // EvtVarTypeUInt32
    EvtChannelPublishingConfigClockType,    // EvtVarTypeUInt32, EVT_CHANNEL_CLOCK_TYPE
    EvtChannelPublishingConfigSidType,      // EvtVarTypeUInt32, EVT_CHANNEL_SID_TYPE
    EvtChannelPublisherList,                // EvtVarTypeString | EVT_VARIANT_TYPE_ARRAY
    EvtChannelConfigPropertyIdEND

} EVT_CHANNEL_CONFIG_PROPERTY_ID;

typedef enum _EVT_CHANNEL_TYPE
{
    EvtChannelTypeAdmin = 0,
    EvtChannelTypeOperational,
    EvtChannelTypeAnalytic,
    EvtChannelTypeDebug

} EVT_CHANNEL_TYPE;

typedef enum _EVT_CHANNEL_ISOLATION_TYPE
{
    EvtChannelIsolationTypeApplication = 0,
    EvtChannelIsolationTypeSystem,
    EvtChannelIsolationTypeCustom

} EVT_CHANNEL_ISOLATION_TYPE;

typedef enum _EVT_CHANNEL_CLOCK_TYPE
{
    EvtChannelClockTypeSystemTime = 0,      // System time
    EvtChannelClockTypeQPC                  // Query performance counter

} EVT_CHANNEL_CLOCK_TYPE;

typedef enum _EVT_CHANNEL_SID_TYPE
{
    EvtChannelSidTypeNone = 0,
    EvtChannelSidTypePublishing

} EVT_CHANNEL_SID_TYPE;

EVT_HANDLE WINAPI EvtOpenChannelEnum(
    EVT_HANDLE Session,
    DWORD Flags
    );

BOOL WINAPI EvtNextChannelPath(
    EVT_HANDLE ChannelEnum,
    DWORD ChannelPathBufferSize,
    __out_ecount_part_opt(ChannelPathBufferSize, *ChannelPathBufferUsed)
    LPWSTR ChannelPathBuffer,
    __out PDWORD ChannelPathBufferUsed
    );

EVT_HANDLE WINAPI EvtOpenChannelConfig(
    EVT_HANDLE Session,
    LPCWSTR ChannelPath,
    DWORD Flags
    );

BOOL WINAPI EvtSaveChannelConfig(
    EVT_HANDLE ChannelConfig,
    DWORD Flags
    );

BOOL WINAPI EvtSetChannelConfigProperty(
    EVT_HANDLE ChannelConfig,
    EVT_CHANNEL_CONFIG_PROPERTY_ID PropertyId,
    DWORD Flags,
    PEVT_VARIANT PropertyValue
    );

BOOL WINAPI EvtGetChannelConfigProperty(
    EVT_HANDLE ChannelConfig,
    EVT_CHANNEL_CONFIG_PROPERTY_ID PropertyId,
    DWORD Flags,
    DWORD PropertyValueBufferSize,
    PEVT_VARIANT PropertyValueBuffer,
    __out PDWORD PropertyValueBufferUsed
    );


////////////////////////////////////////////////////////////////////////////////
//
// Publisher Metadata
//
////////////////////////////////////////////////////////////////////////////////

typedef enum _EVT_CHANNEL_REFERENCE_FLAGS
{
    EvtChannelReferenceImported = 0x1,

} EVT_CHANNEL_REFERENCE_FLAGS;

typedef enum _EVT_PUBLISHER_METADATA_PROPERTY_ID
{
    EvtPublisherMetadataPublisherGuid = 0,      // EvtVarTypeGuid
    EvtPublisherMetadataResourceFilePath,       // EvtVarTypeString
    EvtPublisherMetadataParameterFilePath,      // EvtVarTypeString
    EvtPublisherMetadataMessageFilePath,        // EvtVarTypeString
    EvtPublisherMetadataHelpLink,               // EvtVarTypeString
    EvtPublisherMetadataPublisherMessageID,     // EvtVarTypeUInt32

    EvtPublisherMetadataChannelReferences,      // EvtVarTypeEvtHandle, ObjectArray
    EvtPublisherMetadataChannelReferencePath,   // EvtVarTypeString
    EvtPublisherMetadataChannelReferenceIndex,  // EvtVarTypeUInt32
    EvtPublisherMetadataChannelReferenceID,     // EvtVarTypeUInt32
    EvtPublisherMetadataChannelReferenceFlags,  // EvtVarTypeUInt32
    EvtPublisherMetadataChannelReferenceMessageID, // EvtVarTypeUInt32

    EvtPublisherMetadataLevels,                 // EvtVarTypeEvtHandle, ObjectArray
    EvtPublisherMetadataLevelName,              // EvtVarTypeString
    EvtPublisherMetadataLevelValue,             // EvtVarTypeUInt32
    EvtPublisherMetadataLevelMessageID,         // EvtVarTypeUInt32

    EvtPublisherMetadataTasks,                  // EvtVarTypeEvtHandle, ObjectArray
    EvtPublisherMetadataTaskName,               // EvtVarTypeString
    EvtPublisherMetadataTaskEventGuid,          // EvtVarTypeGuid
    EvtPublisherMetadataTaskValue,              // EvtVarTypeUInt32
    EvtPublisherMetadataTaskMessageID,          // EvtVarTypeUInt32

    EvtPublisherMetadataOpcodes,                // EvtVarTypeEvtHandle, ObjectArray
    EvtPublisherMetadataOpcodeName,             // EvtVarTypeString
    EvtPublisherMetadataOpcodeValue,            // EvtVarTypeUInt32
    EvtPublisherMetadataOpcodeMessageID,        // EvtVarTypeUInt32

    EvtPublisherMetadataKeywords,               // EvtVarTypeEvtHandle, ObjectArray
    EvtPublisherMetadataKeywordName,            // EvtVarTypeString
    EvtPublisherMetadataKeywordValue,           // EvtVarTypeUInt64
    EvtPublisherMetadataKeywordMessageID,       // EvtVarTypeUInt32


    EvtPublisherMetadataPropertyIdEND

} EVT_PUBLISHER_METADATA_PROPERTY_ID;

EVT_HANDLE WINAPI EvtOpenPublisherEnum(
    EVT_HANDLE Session,
    DWORD Flags
    );

BOOL WINAPI EvtNextPublisherId(
    EVT_HANDLE PublisherEnum,
    DWORD PublisherIdBufferSize,
    __out_ecount_part_opt(PublisherIdBufferSize, *PublisherIdBufferUsed)
    LPWSTR PublisherIdBuffer,
    __out PDWORD PublisherIdBufferUsed
    );

#endif

EVT_HANDLE WINAPI EvtOpenPublisherMetadata(
    EVT_HANDLE Session,
    LPCWSTR PublisherId,
    LPCWSTR LogFilePath,
    LCID Locale,
    DWORD Flags
    );

#if 0

BOOL WINAPI EvtGetPublisherMetadataProperty(
    EVT_HANDLE PublisherMetadata,
    EVT_PUBLISHER_METADATA_PROPERTY_ID PropertyId,
    DWORD Flags,
    DWORD PublisherMetadataPropertyBufferSize,
    PEVT_VARIANT PublisherMetadataPropertyBuffer,
    __out PDWORD PublisherMetadataPropertyBufferUsed
    );

////////////////////////////////////////////////////////////////////////////////
//
// Event Metadata Configuratin
//
////////////////////////////////////////////////////////////////////////////////

typedef enum _EVT_EVENT_METADATA_PROPERTY_ID
{
    EventMetadataEventID,       // EvtVarTypeUInt32
    EventMetadataEventVersion,  // EvtVarTypeUInt32
    EventMetadataEventChannel,  // EvtVarTypeUInt32
    EventMetadataEventLevel,    // EvtVarTypeUInt32
    EventMetadataEventOpcode,   // EvtVarTypeUInt32
    EventMetadataEventTask,     // EvtVarTypeUInt32
    EventMetadataEventKeyword,  // EvtVarTypeUInt64
    EventMetadataEventMessageID,// EvtVarTypeUInt32
    EventMetadataEventTemplate, // EvtVarTypeString
    EvtEventMetadataPropertyIdEND

} EVT_EVENT_METADATA_PROPERTY_ID;

EVT_HANDLE WINAPI EvtOpenEventMetadataEnum(
    EVT_HANDLE PublisherMetadata,
    DWORD Flags
    );

EVT_HANDLE WINAPI EvtNextEventMetadata(
    EVT_HANDLE EventMetadataEnum,
    DWORD Flags
    );

BOOL WINAPI EvtGetEventMetadataProperty(
    EVT_HANDLE EventMetadata,
    EVT_EVENT_METADATA_PROPERTY_ID PropertyId,
    DWORD Flags,
    DWORD EventMetadataPropertyBufferSize,
    PEVT_VARIANT EventMetadataPropertyBuffer,
    __out PDWORD EventMetadataPropertyBufferUsed
    );

////////////////////////////////////////////////////////////////////////////////
//
// Array Access
//
////////////////////////////////////////////////////////////////////////////////

typedef HANDLE EVT_OBJECT_ARRAY_PROPERTY_HANDLE;

BOOL WINAPI EvtGetObjectArraySize(
    EVT_OBJECT_ARRAY_PROPERTY_HANDLE ObjectArray,
    __out PDWORD ObjectArraySize
    );

BOOL WINAPI EvtGetObjectArrayProperty(
    EVT_OBJECT_ARRAY_PROPERTY_HANDLE ObjectArray,
    DWORD PropertyId,
    DWORD ArrayIndex,
    DWORD Flags,
    DWORD PropertyValueBufferSize,
    PEVT_VARIANT PropertyValueBuffer,
    __out PDWORD PropertyValueBufferUsed
    );


/////////////////////////////////////////////////////////////////////////////
// 
// Misc Event Consumer Functions 
//
////////////////////////////////////////////////////////////////////////////

typedef enum _EVT_QUERY_PROPERTY_ID
{
    // 
    // list of channels or logfiles indentified in the query. Variant will be
    // array of EvtVarTypeString.
    //
    EvtQueryNames,  

    //
    // Array of EvtVarTypeUInt32, indicating creation status ( Win32 error 
    // code ) for the list of names returned by the EvtQueryNames 
    // property.
    //
    EvtQueryStatuses,     

    EvtQueryPropertyIdEND
 
} EVT_QUERY_PROPERTY_ID;

typedef enum _EVT_EVENT_PROPERTY_ID
{
    EvtEventQueryIDs = 0,
    EvtEventPath,
    EvtEventPropertyIdEND

} EVT_EVENT_PROPERTY_ID;


BOOL WINAPI EvtGetQueryInfo(
    EVT_HANDLE QueryOrSubscription,
    EVT_QUERY_PROPERTY_ID PropertyId,
    DWORD PropertyValueBufferSize,
    PEVT_VARIANT PropertyValueBuffer,
    __out PDWORD PropertyValueBufferUsed
    );

EVT_HANDLE WINAPI EvtCreateBookmark(
    __in_opt LPCWSTR BookmarkXml
    );

BOOL WINAPI EvtUpdateBookmark(
    EVT_HANDLE Bookmark,
    EVT_HANDLE Event
    );

BOOL WINAPI EvtGetEventInfo(
    EVT_HANDLE Event,
    EVT_EVENT_PROPERTY_ID PropertyId,
    DWORD PropertyValueBufferSize,
    PEVT_VARIANT PropertyValueBuffer,
    __out PDWORD PropertyValueBufferUsed
    );


////////////////////////////////////////////////////////////////////////////////
//
// Access Control Permissions
//
////////////////////////////////////////////////////////////////////////////////

#define EVT_READ_ACCESS    0x1
#define EVT_WRITE_ACCESS   0x2
#define EVT_CLEAR_ACCESS   0x4
#define EVT_ALL_ACCESS     0x7

#endif

#endif // __WINEVT_H__
