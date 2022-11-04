#ifndef COMMON_FUNCTIONS
#define COMMON_FUNCTIONS

#include <stdio.h>     // Import for `printf` & `perror`
#include <unistd.h>    // Import for `read`, `write & `lseek`
#include <string.h>    // Import for string functions
#include <stdbool.h>   // Import for `bool` data type
#include <sys/types.h> // Import for `open`, `lseek`
#include <sys/stat.h>  // Import for `open`
#include <fcntl.h>     // Import for `open`
#include <stdlib.h>    // Import for `atoi`
#include <errno.h>     // Import for `errno`

#include "../record-struct/account.h"//import account file
#include "../record-struct/customer.h"//import customer file
#include "../record-struct/transaction.h"//import transaction file
#include "./admin-credentials.h"//import admin credentials file
#include "./server-constants.h"//import server constants file

// Function Prototypes ----------------------------------------

bool login_handler(bool isAdmin, int connFD, struct Customer *ptrToCustomer);
bool account_details(int connFD, struct Account *customerAccount);
bool customer_details(int connFD, int customerID);


// Function Definition -----------------------------------


//login handler function
bool login_handler(bool isAdmin, int connFD, struct Customer *ptrToCustomerID)
{
    ssize_t readBytes, writeBytes;            // Number of bytes written to / read from the socket
    char readBuff[1000], writeBuff[1000]; // Buffer for reading from / writing to the client
    char tempBuff[1000];
    struct Customer customer;

    int ID;

    //empty read and write buffer
    bzero(readBuff, sizeof(readBuff));
    bzero(writeBuff, sizeof(writeBuff));

    // Get login message for respective user type
    if (isAdmin)//admin
        strcpy(writeBuff, ADMIN_LOGIN_WELCOME);
    else//customer
        strcpy(writeBuff, CUSTOMER_LOGIN_WELCOME);

    // Append the request for LOGIN ID message
    strcat(writeBuff, "\n");
    strcat(writeBuff, LOGIN_ID);

    writeBytes = write(connFD, writeBuff, strlen(writeBuff));
    if (writeBytes == -1)//Error in writing WELCOME & LOGIN_ID message to the client
    {
        perror("Error in writing WELCOME & LOGIN_ID message to the client!");
        return false;
    }

    readBytes = read(connFD, readBuff, sizeof(readBuff));
    if (readBytes == -1)//Error in reading login ID from client
    {
        perror("Error in reading login ID from client!");
        return false;
    }

    bool userFound = false;

    if (isAdmin)//if admin
    {
        if (strcmp(readBuff, ADMIN_LOGIN_ID) == 0)
            userFound = true;
    }
    else//if customer
    {
        bzero(tempBuff, sizeof(tempBuff));//empty temp buffer
        strcpy(tempBuff, readBuff);
        strtok(tempBuff, "-");
        ID = atoi(strtok(NULL, "-"));

        int cusFileFD = open(CUSTOMER_FILE, O_RDONLY);//customer file file descriptor
        if (cusFileFD == -1)//Error in opening customer file in read mode
        {
            perror("Error in opening customer file in read mode!");
            return false;
        }

        off_t offset = lseek(cusFileFD, ID * sizeof(struct Customer), SEEK_SET);
        if (offset >= 0)
        {
            struct flock lock = {F_RDLCK, SEEK_SET, ID * sizeof(struct Customer), sizeof(struct Customer), getpid()};//defining lock

            int lockingStat = fcntl(cusFileFD, F_SETLKW, &lock);//locking and storing lock status
            if (lockingStat == -1)//Error in obtaining read lock on customer record
            {
                perror("Error in obtaining read lock on customer record!");
                return false;
            }

            readBytes = read(cusFileFD, &customer, sizeof(struct Customer));
            if (readBytes == -1)//Error in reading customer record from file
            {
                ;
                perror("Error in reading customer record from file!");
            }

            lock.l_type = F_UNLCK;//lock type unlock
            fcntl(cusFileFD, F_SETLK, &lock);//unlocking

            if (strcmp(customer.login, readBuff) == 0)//if user found
                userFound = true;

            close(cusFileFD);//close file file descriptor
        }
        else
        {
            writeBytes = write(connFD, CUSTOMER_LOGIN_ID_DOESNT_EXIT, strlen(CUSTOMER_LOGIN_ID_DOESNT_EXIT));
        }
    }

    if (userFound)
    {
        bzero(writeBuff, sizeof(writeBuff));//empty write buffer
        writeBytes = write(connFD, PASSWORD, strlen(PASSWORD));
        if (writeBytes == -1)//Error in writing PASSWORD message to client
        {
            perror("Error in writing PASSWORD message to client!");
            return false;
        }

        bzero(readBuff, sizeof(readBuff));//empty read buffer
        readBytes = read(connFD, readBuff, sizeof(readBuff));
        if (readBytes == 1)//Error in reading password from the client
        {
            perror("Error in reading password from the client!");
            return false;
        }

        char hashedPassword[1000];
        strcpy(hashedPassword, crypt(readBuff, SALT_BAE));

        if (isAdmin)
        {
            if (strcmp(hashedPassword, ADMIN_PASSWORD) == 0)//comparing admmin password
                return true;
        }
        else
        {
            if (strcmp(hashedPassword, customer.password) == 0)// comparing customer password
            {
                *ptrToCustomerID = customer;
                return true;
            }
        }

        bzero(writeBuff, sizeof(writeBuff));//empty write buffer
        writeBytes = write(connFD, INVALID_PASSWORD, strlen(INVALID_PASSWORD));
    }
    else
    {
        bzero(writeBuff, sizeof(writeBuff));//empty write buffer
        writeBytes = write(connFD, INVALID_LOGIN, strlen(INVALID_LOGIN));
    }

    return false;
}


//get account details function
bool account_details(int connFD, struct Account *customerAccount)
{
    ssize_t readBytes, writeBytes;            // Number of bytes read from / written to the socket
    char readBuff[1000], writeBuff[1000]; // A buffer for reading from / writing to the socket
    char tempBuff[1000];

    int accNo; //account number
    struct Account account; //account
    int accFD;// account file descriptor

    if (customerAccount == NULL)
    {

        writeBytes = write(connFD, GET_ACCOUNT_NUMBER, strlen(GET_ACCOUNT_NUMBER));
        if (writeBytes == -1)//Error in writing GET_ACCOUNT_NUMBER message to client
        {
            perror("Error in writing GET_ACCOUNT_NUMBER message to client!");
            return false;
        }

        bzero(readBuff, sizeof(readBuff));//empty read buffer
        readBytes = read(connFD, readBuff, sizeof(readBuff));
        if (readBytes == -1)//Error in reading account number response from client
        {
            perror("Error in reading account number response from client!");
            return false;
        }

        accNo = atoi(readBuff);
    }
    else
        accNo = customerAccount->accountNumber;

    accFD = open(ACCOUNT_FILE, O_RDONLY);// opening account file and storing account file descriptor
    if (accFD == -1)
    {
        // Account record doesn't exist
        bzero(writeBuff, sizeof(writeBuff));//empty buffer
        strcpy(writeBuff, ACCOUNT_ID_DOESNT_EXIT);
        strcat(writeBuff, "^");
        perror("Error in opening account file in account_details!");//Error in opening account file in account_details!
        writeBytes = write(connFD, writeBuff, strlen(writeBuff));
        if (writeBytes == -1)//Error in writing ACCOUNT_ID_DOESNT_EXIT message to client
        {
            perror("Error in writing ACCOUNT_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
        return false;
    }

    int offset = lseek(accFD, accNo * sizeof(struct Account), SEEK_SET);
    if (offset == -1 && errno == EINVAL)
    {
        // Account record doesn't exist
        bzero(writeBuff, sizeof(writeBuff));//empty write buffer
        strcpy(writeBuff, ACCOUNT_ID_DOESNT_EXIT);
        strcat(writeBuff, "^");
        perror("Error seeking to account record in account_details!");
        writeBytes = write(connFD, writeBuff, strlen(writeBuff));
        if (writeBytes == -1)//Error in writing ACCOUNT_ID_DOESNT_EXIT message to client
        {
            perror("Error in writing ACCOUNT_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
        return false;
    }
    else if (offset == -1)//Error in seeking to required account record
    {
        perror("Error in seeking to required account record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};//defining lock

    int lockingStat = fcntl(accFD, F_SETLKW, &lock);//locking
    if (lockingStat == -1)//Error in obtaining read lock on account record
    {
        perror("Error in obtaining read lock on account record!");
        return false;
    }

    readBytes = read(accFD, &account, sizeof(struct Account));
    if (readBytes == -1)//Error in reading account record from file
    {
        perror("Error in reading account record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;//lock type as unlock
    fcntl(accFD, F_SETLK, &lock);//unlocking

    if (customerAccount != NULL)
    {
        *customerAccount = account;
        return true;
    }

    bzero(writeBuff, sizeof(writeBuff));//empty write buffer
    sprintf(writeBuff, "Account Details - \n\tAccount Number : %d\n\tAccount Type : %s\n\tAccount Status : %s", account.accountNumber, (account.isRegularAccount ? "Regular" : "Joint"), (account.active) ? "Active" : "Deactived");
    if (account.active)
    {//account balance
        sprintf(tempBuff, "\n\tAccount Balance:₹ %ld", account.balance);
        strcat(writeBuff, tempBuff);
    }

    sprintf(tempBuff, "\n\tPrimary Owner ID: %d", account.owners[0]);//primary owner id
    strcat(writeBuff, tempBuff);
    if (account.owners[1] != -1)//if joint account
    {
        sprintf(tempBuff, "\n\tSecondary Owner ID: %d", account.owners[1]);//secondary owner id
        strcat(writeBuff, tempBuff);
    }

    strcat(writeBuff, "\n^");

    writeBytes = write(connFD, writeBuff, strlen(writeBuff));
    readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read

    return true;
}


//get customer details
bool customer_details(int connFD, int customerID)
{
    ssize_t readBytes, writeBytes;             // Number of bytes read from / written to the socket
    char readBuff[1000], writeBuff[10000]; // A buffer for reading from / writing to the socket
    char tempBuff[1000];

    struct Customer customer;
    int customerFD;//customer file descriptor
    struct flock lock = {F_RDLCK, SEEK_SET, 0, sizeof(struct Account), getpid()};//defining lock

    if (customerID == -1)
    {
        writeBytes = write(connFD, GET_CUSTOMER_ID, strlen(GET_CUSTOMER_ID));
        if (writeBytes == -1)//Error in writing GET_CUSTOMER_ID message to client
        {
            perror("Error in writing GET_CUSTOMER_ID message to client!");
            return false;
        }

        bzero(readBuff, sizeof(readBuff));//empty read buffer
        readBytes = read(connFD, readBuff, sizeof(readBuff));
        if (readBytes == -1)//Error in getting customer ID from client
        {
            perror("Error in getting customer ID from client!");
            
            return false;
        }

        customerID = atoi(readBuff);//customer id
    }

    customerFD = open(CUSTOMER_FILE, O_RDONLY);//opening customer file and storing customer file descriptor
    if (customerFD == -1)
    {
        // Customer File doesn't exist
        bzero(writeBuff, sizeof(writeBuff));//empty write buffer
        strcpy(writeBuff, CUSTOMER_ID_DOESNT_EXIT);
        strcat(writeBuff, "^");
        writeBytes = write(connFD, writeBuff, strlen(writeBuff));
        if (writeBytes == -1)//Error in writing CUSTOMER_ID_DOESNT_EXIT message to client
        {
            perror("Error in writing CUSTOMER_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
        return false;
    }
    int offset = lseek(customerFD, customerID * sizeof(struct Customer), SEEK_SET);
    if (errno == EINVAL)
    {
        // Customer record doesn't exist
        bzero(writeBuff, sizeof(writeBuff));//empty write buffer
        strcpy(writeBuff, CUSTOMER_ID_DOESNT_EXIT);
        strcat(writeBuff, "^");
        writeBytes = write(connFD, writeBuff, strlen(writeBuff));
        if (writeBytes == -1)//Error in writing CUSTOMER_ID_DOESNT_EXIT message to client
        {
            perror("Error in writing CUSTOMER_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
        return false;
    }
    else if (offset == -1)//Error in seeking to required customer record
    {
        perror("Error in seeking to required customer record!");
        return false;
    }
    lock.l_start = offset;

    int lockingStat = fcntl(customerFD, F_SETLKW, &lock);//defining lock
    if (lockingStat == -1)//Error in obtaining read lock on the Customer file
    {
        perror("Error in obtaining read lock on the Customer file!");
        return false;
    }

    readBytes = read(customerFD, &customer, sizeof(struct Customer));
    if (readBytes == -1)//Error in reading customer record from file
    {
        perror("Error in reading customer record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;//lock type as unlock
    fcntl(customerFD, F_SETLK, &lock);//unlocking

    bzero(writeBuff, sizeof(writeBuff));//empty write buffer
    sprintf(writeBuff, "Customer Details - \n\tID : %d\n\tName : %s\n\tGender : %c\n\tAge: %d\n\tAccount Number : %d\n\tLoginID : %s", customer.id, customer.name, customer.gender, customer.age, customer.account, customer.login);

    strcat(writeBuff, "\n\nYou will now be redirected to main menu.....^");

    writeBytes = write(connFD, writeBuff, strlen(writeBuff));
    if (writeBytes == -1)//Error in writing customer info to client
    {
        perror("Error in writing customer info to client!");
        return false;
    }

    readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
    return true;
}

//get transaction details
bool get_transaction_details(int connFD, int accountNumber)
{

    ssize_t readBytes, writeBytes;// Number of bytes read from or written to the socket
    char readBuff[1000], writeBuff[10000], tempBuff[1000]; // A buffer for reading from or writing to the socket

    struct Account account;

    if (accountNumber == -1)
    {
        // Get the accountNumber
        writeBytes = write(connFD, GET_ACCOUNT_NUMBER, strlen(GET_ACCOUNT_NUMBER));
        if (writeBytes == -1)//Error in writing GET_ACCOUNT_NUMBER message to client
        {
            perror("Error in writing GET_ACCOUNT_NUMBER message to client!");
            return false;
        }

        bzero(readBuff, sizeof(readBuff));//empty read buffer
        readBytes = read(connFD, readBuff, sizeof(readBuff));
        if (readBytes == -1)//Error in reading account number response from client
        {
            perror("Error in reading account number response from client!");
            return false;
        }

        account.accountNumber = atoi(readBuff);
    }
    else
        account.accountNumber = accountNumber;

    if (account_details(connFD, &account))
    {
        int iter;

        struct Transaction trans;
        struct tm transTime;

        bzero(writeBuff, sizeof(readBuff));//empty write buffer

        int transFD = open(TRANSACTION_FILE, O_RDONLY);//opening transaction file and storing transaction file descriptor
        if (transFD == -1)//Error in opening transaction file
        {
            perror("Error in opening transaction file!");
            write(connFD, TRANSACTIONS_NOT_FOUND, strlen(TRANSACTIONS_NOT_FOUND));
            read(connFD, readBuff, sizeof(readBuff)); // Dummy read
            return false;
        }

        for (iter = 0; iter < MAX_TRANSACTIONS && account.transactions[iter] != -1; iter++)
        {

            int offset = lseek(transFD, account.transactions[iter] * sizeof(struct Transaction), SEEK_SET);
            if (offset == -1)//Error in seeking to required transaction record
            {
                perror("Error in seeking to required transaction record!");
                return false;
            }

            struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Transaction), getpid()};// defining lock

            int lockingStat = fcntl(transFD, F_SETLKW, &lock);//locking
            if (lockingStat == -1)//Error in obtaining read lock on transaction record
            {
                perror("Error in obtaining read lock on transaction record!");
                return false;
            }

            readBytes = read(transFD, &trans, sizeof(struct Transaction));
            if (readBytes == -1)//Error in reading transaction record from file
            {
                perror("Error in reading transaction record from file!");
                return false;
            }

            lock.l_type = F_UNLCK;//lock type as unlock
            fcntl(transFD, F_SETLK, &lock);//unlocking

            transTime = *localtime(&(trans.transactionTime));//transaction time

            bzero(tempBuff, sizeof(tempBuff));//empty temp buffer 
           
            //printing transaction details
            sprintf(tempBuff, "Details of transaction %d - \n\t Date : %d:%d %d/%d/%d \n\t Operation : %s \n\t Balance - \n\t\t Before : %ld \n\t\t After : %ld \n\t\t Difference : %ld\n", (iter + 1), transTime.tm_hour, transTime.tm_min, transTime.tm_mday, transTime.tm_mon, transTime.tm_year, (trans.operation ? "Deposit" : "Withdraw"), trans.oldBalance, trans.newBalance, (trans.newBalance - trans.oldBalance));

            if (strlen(writeBuff) == 0)
                strcpy(writeBuff, tempBuff);
            else
                strcat(writeBuff, tempBuff);
        }

        close(transFD);//closing transaction file descriptor

        if (strlen(writeBuff) == 0)
        {
            write(connFD, TRANSACTIONS_NOT_FOUND, strlen(TRANSACTIONS_NOT_FOUND));
            read(connFD, readBuff, sizeof(readBuff)); // Dummy read
            return false;
        }
        else
        {
            strcat(writeBuff, "^");
            writeBytes = write(connFD, writeBuff, strlen(writeBuff));
            read(connFD, readBuff, sizeof(readBuff)); // Dummy read
        }
    }
}
// --------------------------------------------------------------------------------------------------

#endif