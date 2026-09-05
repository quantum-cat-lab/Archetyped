#pragma once
#include "SqlInstance.h"
#include "FURCMD/FURCMD.h"
#include <SDK/archetyped/hash/hash.h>
#include "ankerl/unordered_dense.h"
#include <cstdint>

/**
 * registerSQLDomainContext - Context for registering a SQL domain.
 */
struct registerSQLDomainContext {
    uint32_t domainId;

    const char* domainName;
};

/**
 * openInstanceCMDContext - Context for opening a SQL database.
 */
struct openInstanceCMDContext {

    uint32_t domainId;

    const char* dbPath;

};

/**
 * executeInstanceCMDContext - Context for executing a SQL command.
 */
struct executeInstanceCMDContext {
    uint32_t domainId;

    const char* sql;
};

/**
 * setStringInstanceCMDContext - Context for setting a string value in a SQL domain.
 */
struct setStringInstanceCMDContext {
    uint32_t domainId;

    const char* key;

    const char* value;
};

/**
 * getStringInstanceCMDContext - Context for getting a string value from a SQL domain.
 */
struct getStringInstanceCMDContext {
    uint32_t domainId;

    const char* key;

    uint32_t bufferSize;

};

/**
 * existsInstanceCMDContext - Context for checking if a key exists in a SQL domain.
 */
struct existsInstanceCMDContext {
    uint32_t domainId;

    const char* key;

};

/**
 * closeInstanceCMDContext - Context for closing a SQL domain.
 */
struct closeInstanceCMDContext {
    uint32_t domainId;

};

/**
 * isOpenInstanceCMDContext - Context for checking if a SQL domain is open.
 */
struct isOpenInstanceCMDContext {
    uint32_t domainId;

};

/**
 * SQLFactory (SQL Domains) - Core Component.
 * Responsible for managing multiple SQLite database domains.
 */
class SQLFactory {
public:
    /**
     * Initializes SQLFactory.
     */
    SQLFactory();
    ~SQLFactory() = default;
    /**
     * Registers a new SQL domain.
     * 
     * @param packet The command packet containing registration data.
     */
    static void registerSQLDomain(FURCMDPacket& packet);
    /**
     * Opens a SQL database for a given domain.
     * 
     * @param packet The command packet containing open request data.
     */
    static void openCMD(FURCMDPacket& packet);
    /**
     * Executes a SQL command in a given domain.
     * 
     * @param packet The command packet containing the SQL command.
     */
    static void executeCMD(FURCMDPacket& packet);
    /**
     * Sets a string value in a given domain.
     * 
     * @param packet The command packet containing the key and value.
     */
    static void setStringCMD(FURCMDPacket& packet);
    /**
     * Gets a string value from a given domain.
     * 
     * @param packet The command packet containing the key and buffer.
     */
    static void getStringCMD(FURCMDPacket& packet);
    /**
     * Checks if a key exists in a given domain.
     * 
     * @param packet The command packet containing the key.
     */
    static void existsCMD(FURCMDPacket& packet);
    /**
     * Closes a SQL database for a given domain.
     * 
     * @param packet The command packet containing the domain ID.
     */
    static void closeCMD(FURCMDPacket& packet);
    /**
     * Checks if a SQL database is open for a given domain.
     * 
     * @param packet The command packet containing the domain ID.
     */
    static void isOpenCMD(FURCMDPacket& packet);
private:
    static ankerl::unordered_dense::map<uint32_t, SqlInstance*, IdentityHash> sqlDomains;

};
