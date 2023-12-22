# final-project-44

### Team Information

#### Team Number : 44
#### Team Members:

- Amey Choudhary , 2021113017
- Jainit Bafna ,  2021114003
- Yash Bhaskar ,  2021114012

#### Disclaimer: We have used AI-powered tools, like ChatGPT and GitHub Copilot.

Given the complexity and length of a complete implementation for the described Network File System (NFS) with Naming and Storage servers, it's impractical to write the entire code within this format. However, I can guide you through a high-level approach, describe the modules you'll need, outline the functions that each module should contain, and give you code snippets or pseudocode for key parts. You can then use this as a blueprint to implement the entire system.


High-Level Modules
Client Module - handles client interactions, such as reading, writing, and obtaining information about files.
Naming Server Module - the central server that manages directories and file locations.
Storage Server Module - responsible for the actual data storage and file operations.
Communication Module - handles all network communication using TCP sockets.
Concurrency Module - handles concurrent client access and file operations.
Error Handling Module - defines and processes error codes.
Search Optimization Module - optimizes search operations within the Naming Server.
Redundancy Module - handles redundancy and replication of data.
Logging Module - for logging and message display.


Client Module Functions
connectToNamingServer()
sendReadRequest()
sendWriteRequest()
sendInfoRequest()
sendCreateRequest()
sendDeleteRequest()
sendCopyRequest()


Naming Server Module Functions
initializeNamingServer()
registerStorageServer()
handleClientRequest()
instructStorageServer()
searchForFile()
cacheSearchResult()
detectStorageServerFailure()
handleReplication()


Storage Server Module Functions
initializeStorageServer()
reportToNamingServer()
handleRequestFromNamingServer()
readFile()
writeFile()
getFileInfo()
createFileOrDirectory()
deleteFileOrDirectory()
copyFileOrDirectory()


Communication Module Functions (Using TCP Sockets)
openSocket()
acceptConnection()
readFromSocket()
writeToSocket()
closeSocket()



Concurrency Module Functions
processMultipleRequests()
lockFileForWrite()
unlockFile()



Error Handling Module Functions
defineErrorCodes()
handleError()



Search Optimization Module Functions
performEfficientSearch()
implementLRUCache()



Redundancy Module Functions
replicateData()
restoreFromReplica()
asynchronousReplication()



Logging Module Functions
logRequest()
logAck()
logError()
displayMessage()
