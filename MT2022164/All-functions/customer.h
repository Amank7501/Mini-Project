#ifndef CUSTOMER_FUNCTIONS
#define CUSTOMER_FUNCTIONS

// Semaphores are necessary joint account due the design choice I've made
#include <sys/ipc.h>
#include <sys/sem.h>

struct Customer loggedInCustomer;
int semId;

// Function Prototypes -----------------------

bool customer_operation_handler(int connFD);
bool deposit(int connFD);
bool withdraw(int connFD);
bool get_balance(int connFD);
bool change_password(int connFD);
bool lock_CS(struct sembuf *semOp);
bool unlock_CS(struct sembuf *sem_op);
void write_transaction_to_array(int *transactionArray, int ID);
int write_transaction_to_file(int accountNumber, long int oldBalance, long int newBalance, bool operation);


// Function Definition --------------------------------------


//customer operation handler function
bool customer_operation_handler(int connFD)
{
    if (login_handler(false, connFD, &loggedInCustomer))
    {
        ssize_t writeBytes, readBytes;// Number of bytes read from or written to the client
        char readBuff[1000], writeBuff[1000]; // A buffer used for reading & writing to the client

        // Get a semaphore for the user
        key_t semKey = ftok(CUSTOMER_FILE, loggedInCustomer.account); // Generate a key based on the account number hence, different customers will have different semaphores

        union semun
        {
            int val; // Value of the semaphore
        } semSet;//semaphore

        int semctlStat;
        semId = semget(semKey, 1, 0); // Get the semaphore if it exists
        if (semId == -1)//no semaphore exists
        {
            semId = semget(semKey, 1, IPC_CREAT | 0700); // Create a new semaphore
            if (semId == -1)//Error in creating semaphore
            {
                perror("Error in creating semaphore!");
                _exit(1);
            }

            semSet.val = 1; // Set a binary semaphore
            semctlStat = semctl(semId, 0, SETVAL, semSet);
            if (semctlStat == -1)//Error in initializing a binary sempahore
            {
                perror("Error in initializing a binary sempahore!");
                _exit(1);
            }
        }

        bzero(writeBuff, sizeof(writeBuff));//empty write buffer
        strcpy(writeBuff, CUSTOMER_LOGIN_SUCCESS);
        while (1)
        {
            strcat(writeBuff, "\n");
            strcat(writeBuff, CUSTOMER_MENU);
            writeBytes = write(connFD, writeBuff, strlen(writeBuff));
            if (writeBytes == -1)//Error in writing CUSTOMER_MENU to client
            {
                perror("Error in writing CUSTOMER_MENU to client!");
                return false;
            }
            bzero(writeBuff, sizeof(writeBuff));//empty write buffer

            bzero(readBuff, sizeof(readBuff));//empty read buffer
            readBytes = read(connFD, readBuff, sizeof(readBuff));
            if (readBytes == -1)//Error in reading client's choice for CUSTOMER_MENU
            {
                perror("Error in reading client's choice for CUSTOMER_MENU");
                return false;
            }
            
            
            int choice = atoi(readBuff);
            
            switch (choice)
            {
            case 1:
                customer_details(connFD, loggedInCustomer.id);//get customer details
                break;
            case 2:
                deposit(connFD);//deposit
                break;
            case 3:
                withdraw(connFD);//withdraw
                break;
            case 4:
                get_balance(connFD);//get balance
                break;
            case 5:
                get_transaction_details(connFD, loggedInCustomer.account);//get transaction details
                break;
            case 6:
                change_password(connFD);//change password
                break;
            default:
                writeBytes = write(connFD, CUSTOMER_LOGOUT, strlen(CUSTOMER_LOGOUT));
                return false;
            }
        }
    }
    else
    {
        // customer login failed---
        return false;
    }
    return true;
}


//deposit function
bool deposit(int connFD)
{
    char readBuff[1000], writeBuff[1000];
    ssize_t readBytes, writeBytes;

    struct Account acc;
    acc.accountNumber = loggedInCustomer.account;

    long int depositAmt = 0;

    // Lock the critical section
    struct sembuf semOp;
    lock_CS(&semOp);//locking cs

    if (account_details(connFD, &acc))
    {
        
        if (acc.active)//if account is active
        {

            writeBytes = write(connFD, DEPOSIT_AMOUNT, strlen(DEPOSIT_AMOUNT));
            if (writeBytes == -1)//Error in writing DEPOSIT_AMOUNT to client
            {
                perror("Error in writing DEPOSIT_AMOUNT to client!");
                unlock_CS(&semOp);
                return false;
            }

            bzero(readBuff, sizeof(readBuff));//empty read buffer
            readBytes = read(connFD, readBuff, sizeof(readBuff));
            if (readBytes == -1)//Error in reading deposit money from client
            {
                perror("Error in reading deposit money from client!");
                unlock_CS(&semOp);
                return false;
            }

            depositAmt = atol(readBuff);//deposit amount
            if (depositAmt != 0)
            {

                int newTransID = write_transaction_to_file(acc.accountNumber, acc.balance, acc.balance + depositAmt, 1);
                write_transaction_to_array(acc.transactions, newTransID);

                acc.balance += depositAmt;//account balance

                int accFD = open(ACCOUNT_FILE, O_WRONLY);//opening account file and storing file descriptor
                off_t offset = lseek(accFD, acc.accountNumber * sizeof(struct Account), SEEK_SET);

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};//lock details
                int lockingStat = fcntl(accFD, F_SETLKW, &lock);//locking and storing status
                if (lockingStat == -1)//Error in obtaining write lock on account file
                {
                    perror("Error in obtaining write lock on account file!");
                    unlock_CS(&semOp);
                    return false;
                }

                writeBytes = write(accFD, &acc, sizeof(struct Account));
                if (writeBytes == -1)//Error in storing updated deposit money in account record
                {
                    perror("Error in storing updated deposit money in account record!");
                    unlock_CS(&semOp);
                    return false;
                }

                lock.l_type = F_UNLCK;//lock type as unlock
                fcntl(accFD, F_SETLK, &lock);//unlocking

                write(connFD, DEPOSIT_AMOUNT_SUCCESS, strlen(DEPOSIT_AMOUNT_SUCCESS));
                read(connFD, readBuff, sizeof(readBuff)); // Dummy read

                get_balance(connFD);//get balance

                unlock_CS(&semOp);//unlocking critical section

                return true;
            }
            else
                writeBytes = write(connFD, DEPOSIT_AMOUNT_INVALID, strlen(DEPOSIT_AMOUNT_INVALID));
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED, strlen(ACCOUNT_DEACTIVATED));
        read(connFD, readBuff, sizeof(readBuff)); // Dummy read

        unlock_CS(&semOp);//unlocking critical section
    }
    else
    {
        // Fail to get account details
        unlock_CS(&semOp);
        return false;
    }
}


//withdraw function
bool withdraw(int connFD)
{
    char readBuff[1000], writeBuff[1000];
    ssize_t readBytes, writeBytes;

    struct Account acc;
    acc.accountNumber = loggedInCustomer.account;

    long int withdrawAmt = 0;

    // Lock the critical section
    struct sembuf semOp;
    lock_CS(&semOp);//locking critical section

    if (account_details(connFD, &acc))
    {
        if (acc.active)//if account is active
        {

            writeBytes = write(connFD, WITHDRAW_AMOUNT, strlen(WITHDRAW_AMOUNT));
            if (writeBytes == -1)//Error in writing WITHDRAW_AMOUNT message to client
            {
                perror("Error in writing WITHDRAW_AMOUNT message to client!");
                unlock_CS(&semOp);//unlocking critical section
                return false;
            }

            bzero(readBuff, sizeof(readBuff));//empty read buffer
            readBytes = read(connFD, readBuff, sizeof(readBuff));
            if (readBytes == -1)//Error in reading withdraw amount from client
            {
                perror("Error in reading withdraw amount from client!");
                unlock_CS(&semOp);//unlocking critical section
                return false;
            }

            withdrawAmt = atol(readBuff);//withdraw amount

            if (withdrawAmt != 0 && acc.balance - withdrawAmt >= 0)
            {

                int newTransactionID = write_transaction_to_file(acc.accountNumber, acc.balance, acc.balance - withdrawAmt, 0);
                write_transaction_to_array(acc.transactions, newTransactionID);

                acc.balance -= withdrawAmt;//deducting the withdraw amount to main balance

                int accFD = open(ACCOUNT_FILE, O_WRONLY);//opening account file and storing file descriptor 
                off_t offset = lseek(accFD, acc.accountNumber * sizeof(struct Account), SEEK_SET);

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};//defining lock
                int lockingStat = fcntl(accFD, F_SETLKW, &lock);//locking
                if (lockingStat == -1)//Error in obtaining write lock on account record
                {
                    perror("Error in obtaining write lock on account record!");
                    unlock_CS(&semOp);//unlocking critical section
                    return false;
                }

                writeBytes = write(accFD, &acc, sizeof(struct Account));
                if (writeBytes == -1)//Error in writing updated balance into account file
                {
                    perror("Error in writing updated balance into account file!");
                    unlock_CS(&semOp);//unlocking critical section
                    return false;
                }

                lock.l_type = F_UNLCK;//lock type as unlock
                fcntl(accFD, F_SETLK, &lock);//unlocking

                write(connFD, WITHDRAW_AMOUNT_SUCCESS, strlen(WITHDRAW_AMOUNT_SUCCESS));
                read(connFD, readBuff, sizeof(readBuff)); // Dummy read

                get_balance(connFD);//get balance

                unlock_CS(&semOp);//unlocking critical section

                return true;
            }
            else
                writeBytes = write(connFD, WITHDRAW_AMOUNT_INVALID, strlen(WITHDRAW_AMOUNT_INVALID));
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED, strlen(ACCOUNT_DEACTIVATED));
        read(connFD, readBuff, sizeof(readBuff)); // Dummy read

        unlock_CS(&semOp);//unlocking critical section
    }
    else
    {
        // FAILURE while getting account information
        unlock_CS(&semOp);
        return false;
    }
}


//get balance function
bool get_balance(int connFD)
{
    char buff[1000];
    struct Account acc;
    acc.accountNumber = loggedInCustomer.account;
    if (account_details(connFD, &acc))
    {
        bzero(buff, sizeof(buff));//empty buffer
        if (acc.active)//if account is active
        {
            sprintf(buff, "You have ₹ %ld imaginary money in our bank!^", acc.balance);
            write(connFD, buff, strlen(buff));
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED, strlen(ACCOUNT_DEACTIVATED));
        read(connFD, buff, sizeof(buff)); // Dummy read
    }
    else
    {
        // ERROR in getting balance
        return false;
    }
}


//change password function
bool change_password(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuff[1000], writeBuff[1000], hashedPwd[1000];

    char newPwd[1000];

    // Lock the critical section
    struct sembuf semOp = {0, -1, SEM_UNDO};//semaphore
    int semopStat = semop(semId, &semOp, 1);
    if (semopStat == -1)//Error in locking critical section
    {
        perror("Error in locking critical section");
        return false;
    }

    writeBytes = write(connFD, PASSWORD_CHANGE_OLD_PASS, strlen(PASSWORD_CHANGE_OLD_PASS));
    if (writeBytes == -1)//Error in writing PASSWORD_CHANGE_OLD_PASS message to client
    {
        perror("Error in writing PASSWORD_CHANGE_OLD_PASS message to client!");
        unlock_CS(&semOp);//unlocking critical section
        return false;
    }

    bzero(readBuff, sizeof(readBuff));//empty read buffer
    readBytes = read(connFD, readBuff, sizeof(readBuff));
    if (readBytes == -1)//Error in reading old password response from client
    {
        perror("Error in reading old password response from client");
        unlock_CS(&semOp);//unlocking critical section
        return false;
    }

    if (strcmp(crypt(readBuff, SALT_BAE), loggedInCustomer.password) == 0)//comparing customer password
    {
        // Password matches with old password
        writeBytes = write(connFD, PASSWORD_CHANGE_NEW_PASS, strlen(PASSWORD_CHANGE_NEW_PASS));
        if (writeBytes == -1)//Error in writing PASSWORD_CHANGE_NEW_PASS message to client
        {
            perror("Error in writing PASSWORD_CHANGE_NEW_PASS message to client!");
            unlock_CS(&semOp);//unlocking critical section
            return false;
        }
        bzero(readBuff, sizeof(readBuff));//empty read buffer
        readBytes = read(connFD, readBuff, sizeof(readBuff));
        if (readBytes == -1)//Error in reading new password response from client
        {
            perror("Error in reading new password response from client");
            unlock_CS(&semOp);//unlocking critical section
            return false;
        }

        strcpy(newPwd, crypt(readBuff, SALT_BAE));//copying password to new password

        writeBytes = write(connFD, PASSWORD_CHANGE_NEW_PASS_RE, strlen(PASSWORD_CHANGE_NEW_PASS_RE));
        if (writeBytes == -1)//Error in writing PASSWORD_CHANGE_NEW_PASS_RE message to client
        {
            perror("Error in writing PASSWORD_CHANGE_NEW_PASS_RE message to client!");
            unlock_CS(&semOp);//unlocking critical section
            return false;
        }
        bzero(readBuff, sizeof(readBuff));//empty read buffer
        readBytes = read(connFD, readBuff, sizeof(readBuff));
        if (readBytes == -1)//Error in reading new password reenter response from client
        {
            perror("Error in reading new password reenter response from client");
            unlock_CS(&semOp);//unlocking critical section
            return false;
        }

        if (strcmp(crypt(readBuff, SALT_BAE), newPwd) == 0)//compring new password
        {
            // New & reentered passwords match

            strcpy(loggedInCustomer.password, newPwd);

            int customerFD = open(CUSTOMER_FILE, O_WRONLY);//openiing customer file and storing file descriptor
            if (customerFD == -1)//Error in opening customer file
            {
                perror("Error in opening customer file!");
                unlock_CS(&semOp);//unlocking critical section
                return false;
            }

            off_t offset = lseek(customerFD, loggedInCustomer.id * sizeof(struct Customer), SEEK_SET);
            if (offset == -1)//Error in seeking to the customer record
            {
                perror("Error in seeking to the customer record!");
                unlock_CS(&semOp);//unlocking critical section
                return false;
            }

            struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};//defining lock
            int lockingStat = fcntl(customerFD, F_SETLKW, &lock);//locking
            if (lockingStat == -1)//Error in obtaining write lock on customer record
            {
                perror("Error in obtaining write lock on customer record!");
                unlock_CS(&semOp);//unlocking critical section
                return false;
            }

            writeBytes = write(customerFD, &loggedInCustomer, sizeof(struct Customer));
            if (writeBytes == -1)//Error in storing updated customer password into customer record
            {
                perror("Error in storing updated customer password into customer record!");
                unlock_CS(&semOp);//unlocking critical section
                return false;
            }

            lock.l_type = F_UNLCK;//lock type as unlock
            lockingStat = fcntl(customerFD, F_SETLK, &lock);//unlocking

            close(customerFD);//closing customer file descriptor

            writeBytes = write(connFD, PASSWORD_CHANGE_SUCCESS, strlen(PASSWORD_CHANGE_SUCCESS));
            readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read

            unlock_CS(&semOp);//unlocking critical section

            return true;
        }
        else
        {
            // New & reentered passwords don't match
            writeBytes = write(connFD, PASSWORD_CHANGE_NEW_PASS_INVALID, strlen(PASSWORD_CHANGE_NEW_PASS_INVALID));
            readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
        }
    }
    else
    {
        // Password doesn't match with old password
        writeBytes = write(connFD, PASSWORD_CHANGE_OLD_PASS_INVALID, strlen(PASSWORD_CHANGE_OLD_PASS_INVALID));
        readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
    }

    unlock_CS(&semOp);//unlocking critical section

    return false;
}


//lock critical section function
bool lock_CS(struct sembuf *semOp)
{
    semOp->sem_flg = SEM_UNDO;
    semOp->sem_op = -1;
    semOp->sem_num = 0;
    int semopStat = semop(semId, semOp, 1);
    if (semopStat == -1)//Error in locking critical section
    {
        perror("Error in locking critical section");
        return false;
    }
    return true;
}


//unlock critical section function
bool unlock_CS(struct sembuf *semOp)
{
    semOp->sem_op = 1;
    int semopStat = semop(semId, semOp, 1);
    if (semopStat == -1)//Error in operating on semaphore
    {
        perror("Error in operating on semaphore!");
        _exit(1);
    }
    return true;
}


//write transaction to array function
void write_transaction_to_array(int *transArray, int ID)
{
    // Check if there's any free space in the array to write the new transaction ID
    int iter = 0;
    while (transArray[iter] != -1)
        iter++;

    if (iter >= MAX_TRANSACTIONS)
    {
        // No space
        for (iter = 1; iter < MAX_TRANSACTIONS; iter++)
            // Shift elements one step back discarding the oldest transaction
            transArray[iter - 1] = transArray[iter];
        transArray[iter - 1] = ID;
    }
    else
    {
        // Space available
        transArray[iter] = ID;
    }
}


//write transaction to file function
int write_transaction_to_file(int accountNo, long int oldBalance, long int newBalance, bool operation)
{
    struct Transaction newTrans;
    newTrans.accountNumber = accountNo;
    newTrans.oldBalance = oldBalance;
    newTrans.newBalance = newBalance;
    newTrans.operation = operation;
    newTrans.transactionTime = time(NULL);

    ssize_t readBytes, writeBytes;

    int transFD = open(TRANSACTION_FILE, O_CREAT | O_APPEND | O_RDWR, S_IRWXU);//open transaction file and storing file descriptor in transFD

    // Get most recent transaction number
    off_t offset = lseek(transFD, -sizeof(struct Transaction), SEEK_END);
    if (offset >= 0)
    {
        // There exists at least one transaction record
        struct Transaction prevTrans;//previous transaction
        readBytes = read(transFD, &prevTrans, sizeof(struct Transaction));

        newTrans.transactionID = prevTrans.transactionID + 1;
    }
    else
        // No transaction records exist
        newTrans.transactionID = 0;

    writeBytes = write(transFD, &newTrans, sizeof(struct Transaction));

    return newTrans.transactionID;
}

// =====================================================

#endif