#ifndef ADMIN_FUNCTIONS
#define ADMIN_FUNCTIONS

#include "./imp_functions.h"

// Function Prototypes -----------------------------------------
bool admin_operation_handler(int connFD);
bool add_account(int connFD);
int add_customer(int connFD, bool isPrimary, int newAccountNumber);
bool delete_account(int connFD);
bool modify_customer_details(int connFD);

// ---------------------------------------------------------
// Function Definition --------------------------------------


bool admin_operation_handler(int connFD)
{

    if (login_handler(true, connFD, NULL))
    {
        ssize_t writeBytes, readBytes;            // Number of bytes read from or written to the client
        char readBuff[1000], writeBuff[1000]; // buffer for reading & writing to the client
        bzero(writeBuff, sizeof(writeBuff)); //empty write buffer
        strcpy(writeBuff, ADMIN_LOGIN_SUCCESS);//copying ADMIN_LOGIN_SUCCESS to write buffer
        while (1)
        {
            strcat(writeBuff, "\n"); 
            strcat(writeBuff, ADMIN_MENU);//copying ADMIN_MENU to write buffer
            writeBytes = write(connFD, writeBuff, strlen(writeBuff));
            if (writeBytes == -1)//Error in writing ADMIN_MENU to client
            {
                perror("Error in writing ADMIN_MENU to client!");
                return false;
            }
            bzero(writeBuff, sizeof(writeBuff));//empty write buffer

            readBytes = read(connFD, readBuff, sizeof(readBuff));
            if (readBytes == -1)//Error in reading client's choice for ADMIN_MENU
            {
                perror("Error in reading client's choice for ADMIN_MENU");
                
                return false;
            }

            int choice = atoi(readBuff);// user choice
            switch (choice)
            {
            case 1:
                customer_details(connFD, -1);//get customer details
                break;
            case 2:
                account_details(connFD, NULL);//get account details
                break;
            case 3: 
                get_transaction_details(connFD, -1);//get transaction details
                break;
            case 4:
                add_account(connFD);//add account
                break;
            case 5:
                delete_account(connFD);//delete account
                break;
            case 6:
                modify_customer_details(connFD);//modify customer info
                break;
            default:
                writeBytes = write(connFD, ADMIN_LOGOUT, strlen(ADMIN_LOGOUT));//Admin logout
                return false;
            }
        }
    }
    else
    {
        // ADMIN LOGIN FAILED
        return false;
    }
    return true;
}


//add account function
bool add_account(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuff[1000], writeBuff[1000];

    struct Account newAccount, prevAccount;//new and prev account

    int accountFD = open(ACCOUNT_FILE, O_RDONLY);//account file descriptor
    if (accountFD == -1 && errno == ENOENT)
    {
        // Account file was never created
        newAccount.accountNumber = 0;
    }
    else if (accountFD == -1)//Error in opening account file
    {
        perror("Error in opening account file");
        return false;
    }
    else
    {
        int offset = lseek(accountFD, -sizeof(struct Account), SEEK_END);
        if (offset == -1)//Error in seeking to last Account record
        {
            perror("Error in seeking to last Account record!");
            return false;
        }

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};//defining lock
        int lockingStat = fcntl(accountFD, F_SETLKW, &lock);//locking
        if (lockingStat == -1)//Error in obtaining read lock on Account record
        {
            perror("Error in obtaining read lock on Account record!");
            return false;
        }

        readBytes = read(accountFD, &prevAccount, sizeof(struct Account));
        if (readBytes == -1)//Error in reading Account record from file
        {
            perror("Error in reading Account record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;//lock type
        fcntl(accountFD, F_SETLK, &lock);//unlocking

        close(accountFD);//closing account file descriptor

        newAccount.accountNumber = prevAccount.accountNumber + 1;//new account number is 1+ prev account number
    }
    writeBytes = write(connFD, ADMIN_ADD_ACCOUNT_TYPE, strlen(ADMIN_ADD_ACCOUNT_TYPE));
    if (writeBytes == -1)//Error in writing ADMIN_ADD_ACCOUNT_TYPE message to client
    {
        perror("Error in writing ADMIN_ADD_ACCOUNT_TYPE message to client!");
        return false;
    }

    bzero(readBuff, sizeof(readBuff));
    readBytes = read(connFD, &readBuff, sizeof(readBuff));
    if (readBytes == -1)//Error in reading account type response from client
    {
        perror("Error in reading account type response from client!");
        return false;
    }

    newAccount.isRegularAccount = atoi(readBuff) == 1 ? true : false;//is regular account? User choice

    newAccount.owners[0] = add_customer(connFD, true, newAccount.accountNumber);// adding customer account

    if (newAccount.isRegularAccount)
        newAccount.owners[1] = -1;
    else
        newAccount.owners[1] = add_customer(connFD, false, newAccount.accountNumber);//adding new customer account 

    newAccount.active = true;//account status as active
    newAccount.balance = 0;

    memset(newAccount.transactions, -1, MAX_TRANSACTIONS * sizeof(int));

    accountFD = open(ACCOUNT_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);//opening account file and storing file descriptor in accountFD
    if (accountFD == -1)//Error in while creating or opening account file
    {
        perror("Error in while creating or opening account file!");
        return false;
    }

    writeBytes = write(accountFD, &newAccount, sizeof(struct Account));
    if (writeBytes == -1)//Error in writing Account record to file
    {
        perror("Error in writing Account record to file!");
        return false;
    }

    close(accountFD);//closing account file descriptor

    bzero(writeBuff, sizeof(writeBuff));//empty write buffer
    sprintf(writeBuff, "%s%d", ADMIN_ADD_ACCOUNT_NUMBER, newAccount.accountNumber);
    strcat(writeBuff, "\nRedirecting to the main menu .....^");
    writeBytes = write(connFD, writeBuff, sizeof(writeBuff));
    readBytes = read(connFD, readBuff, sizeof(read)); // Dummy read
    return true;
}


//add customer function
int add_customer(int connFD, bool isPrimary, int newAccountNumber)
{
    ssize_t readBytes, writeBytes;
    char readBuff[1000], writeBuff[1000];

    struct Customer newCustomer, prevCustomer;

    int customerFD = open(CUSTOMER_FILE, O_RDONLY);
    if (customerFD == -1 && errno == ENOENT)
    {
        // Customer file was never created
        newCustomer.id = 0;
    }
    else if (customerFD == -1)//Error in opening customer file
    {
        perror("Error in opening customer file");
        return -1;
    }
    else
    {
        int offset = lseek(customerFD, -sizeof(struct Customer), SEEK_END);
        if (offset == -1)//Error in seeking to last Customer record
        {
            perror("Error in seeking to last Customer record!");
            return false;
        }

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};//definig lock
        int lockingStat = fcntl(customerFD, F_SETLKW, &lock);//locking
        if (lockingStat == -1)//Error in obtaining read lock on Customer record
        {
            perror("Error in obtaining read lock on Customer record!");
            return false;
        }

        readBytes = read(customerFD, &prevCustomer, sizeof(struct Customer));
         if (readBytes == -1)//Error in reading Customer record from file
        {
            perror("Error in reading Customer record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;//lock type as unlock
        fcntl(customerFD, F_SETLK, &lock);//unlocking

        close(customerFD);//closing customer file descriptor

        newCustomer.id = prevCustomer.id + 1;//new customer id is 1 more than prev customer id
    }

    if (isPrimary)//primary account message
        sprintf(writeBuff, "%s%s", ADMIN_ADD_CUSTOMER_PRIMARY, ADMIN_ADD_CUSTOMER_NAME);
    else//secondary account message
        sprintf(writeBuff, "%s%s", ADMIN_ADD_CUSTOMER_SECONDARY, ADMIN_ADD_CUSTOMER_NAME);

    writeBytes = write(connFD, writeBuff, sizeof(writeBuff));
    if (writeBytes == -1)//Error in writing ADMIN_ADD_CUSTOMER_NAME message to client
    {
        perror("Error in writing ADMIN_ADD_CUSTOMER_NAME message to client!");
        return false;
    }

    readBytes = read(connFD, readBuff, sizeof(readBuff));
    if (readBytes == -1)//Error in reading customer name response from client
    {
        perror("Error in reading customer name response from client!");
        ;
        return false;
    }

    strcpy(newCustomer.name, readBuff);//copying customer name to its account

    writeBytes = write(connFD, ADMIN_ADD_CUSTOMER_GENDER, strlen(ADMIN_ADD_CUSTOMER_GENDER));
    if (writeBytes == -1)//Error in writing ADMIN_ADD_CUSTOMER_GENDER message to client
    {
        perror("Error in writing ADMIN_ADD_CUSTOMER_GENDER message to client!");
        return false;
    }

    bzero(readBuff, sizeof(readBuff));//empty read buffer
    readBytes = read(connFD, readBuff, sizeof(readBuff));
    if (readBytes == -1)//Error in reading customer gender response from client
    {
        perror("Error in reading customer gender response from client!");
        return false;
    }

    if (readBuff[0] == 'M' || readBuff[0] == 'F' || readBuff[0] == 'O')
        newCustomer.gender = readBuff[0];//gender
    else
    {
        writeBytes = write(connFD, ADMIN_ADD_CUSTOMER_WRONG_GENDER, strlen(ADMIN_ADD_CUSTOMER_WRONG_GENDER));
        readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
        return false;
    }

    bzero(writeBuff, sizeof(writeBuff));//empty write buffer
    strcpy(writeBuff, ADMIN_ADD_CUSTOMER_AGE);
    writeBytes = write(connFD, writeBuff, strlen(writeBuff));
    if (writeBytes == -1)//Error in writing ADMIN_ADD_CUSTOMER_AGE message to client
    {
        perror("Error in writing ADMIN_ADD_CUSTOMER_AGE message to client!");
        return false;
    }

    bzero(readBuff, sizeof(readBuff));//empty read buffer
    readBytes = read(connFD, readBuff, sizeof(readBuff));
    if (readBytes == -1)//Error in reading customer age response from client
    {
        perror("Error in reading customer age response from client!");
        return false;
    }

    int customerAge = atoi(readBuff);//customer age. User input
    if (customerAge == 0)
    {
        // Either client has sent age as 0 (which is invalid) or has entered a non-numeric string
        bzero(writeBuff, sizeof(writeBuff));
        strcpy(writeBuff, ERRON_INPUT_FOR_NUMBER);
        writeBytes = write(connFD, writeBuff, strlen(writeBuff));
        if (writeBytes == -1)//Error in writing ERRON_INPUT_FOR_NUMBER message to client
        {
            perror("Error in writing ERRON_INPUT_FOR_NUMBER message to client!");
            return false;
        }
        readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
        return false;
    }
    newCustomer.age = customerAge;//copying customer age to its account

    newCustomer.account = newAccountNumber;//copying new account number to account

    //format of customer id
    strcpy(newCustomer.login, newCustomer.name);
    strcat(newCustomer.login, "-");
    sprintf(writeBuff, "%d", newCustomer.id);
    strcat(newCustomer.login, writeBuff);


    char hashedPassword[1000];
    strcpy(hashedPassword, crypt(AUTOGEN_PASSWORD, SALT_BAE));//Autogenerated password
    strcpy(newCustomer.password, hashedPassword);

    customerFD = open(CUSTOMER_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);//opening customer file and storing to customer file descriptor
    if (customerFD == -1)//Error in creating / opening customer file
    {
        perror("Error in creating or opening customer file!");
        return false;
    }
    writeBytes = write(customerFD, &newCustomer, sizeof(newCustomer));
    if (writeBytes == -1)//Error in writing Customer record to file
    {
        perror("Error in writing Customer record to file!");
        return false;
    }

    close(customerFD);//closing customer file descriptor

    bzero(writeBuff, sizeof(writeBuff));//empty write buffer
    sprintf(writeBuff, "%s%s-%d\n%s%s", ADMIN_ADD_CUSTOMER_AUTOGEN_LOGIN, newCustomer.name, newCustomer.id, ADMIN_ADD_CUSTOMER_AUTOGEN_PASSWORD, AUTOGEN_PASSWORD);
    strcat(writeBuff, "^");
    writeBytes = write(connFD, writeBuff, strlen(writeBuff));
    if (writeBytes == -1)//Error in sending customer loginID and password to the client
    {
        perror("Error in sending customer loginID and password to the client!");
        return false;
    }

    readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read

    return newCustomer.id;
}

//delete account function
bool delete_account(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuff[1000], writeBuff[1000];

    struct Account account; //account of type struct Account

    writeBytes = write(connFD, ADMIN_DEL_ACCOUNT_NO, strlen(ADMIN_DEL_ACCOUNT_NO));
    if (writeBytes == -1)//Error in writing ADMIN_DEL_ACCOUNT_NO to client
    {
        perror("Error in writing ADMIN_DEL_ACCOUNT_NO to client!");
        return false;
    }

    bzero(readBuff, sizeof(readBuff));//empty read buff
    readBytes = read(connFD, readBuff, sizeof(readBuff));
    if (readBytes == -1)//Error in reading account number response from the client
    {
        perror("Error in reading account number response from the client!");
        return false;
    }

    int accountNumber = atoi(readBuff);//accont number

    int accountFD = open(ACCOUNT_FILE, O_RDONLY);//opening Account file and storing file descriptor in accountFD
    if (accountFD == -1)//error in opening account file
    {
        // Account record doesn't exist
        bzero(writeBuff, sizeof(writeBuff));//empty write buffer
        strcpy(writeBuff, ACCOUNT_ID_DOESNT_EXIT);
        strcat(writeBuff, "^");
        writeBytes = write(connFD, writeBuff, strlen(writeBuff));
        if (writeBytes == -1)//Error in writing ACCOUNT_ID_DOESNT_EXIT message to client
        {
            perror("Error in writing ACCOUNT_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
        return false;
    }


    int offset = lseek(accountFD, accountNumber * sizeof(struct Account), SEEK_SET);
    if (errno == EINVAL)
    {
        // Customer record doesn't exist
        bzero(writeBuff, sizeof(writeBuff));//empty write buffer
        strcpy(writeBuff, ACCOUNT_ID_DOESNT_EXIT);
        strcat(writeBuff, "^");
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
    int lockingStat = fcntl(accountFD, F_SETLKW, &lock);//locking 
    if (lockingStat == -1)//Error in obtaining read lock on Account record
    {
        perror("Error in obtaining read lock on Account record!");
        return false;
    }

    readBytes = read(accountFD, &account, sizeof(struct Account));
    if (readBytes == -1)//Error in reading Account record from file
    {
        perror("Error in reading Account record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;//lock type as unlock
    fcntl(accountFD, F_SETLK, &lock);//unlocking

    close(accountFD);//close account file descriptor

    bzero(writeBuff, sizeof(writeBuff));//empty write buffer
    if (account.balance == 0)
    {
        // No money, hence can close account
        account.active = false;
        accountFD = open(ACCOUNT_FILE, O_WRONLY);//opening Account file and storing file descriptor in accountFD
        if (accountFD == -1)//Error in opening Account file in write mode
        {
            perror("Error in opening Account file in write mode!");
            return false;
        }

        offset = lseek(accountFD, accountNumber * sizeof(struct Account), SEEK_SET);
        if (offset == -1)//Error in seeking to the Account
        {
            perror("Error in seeking to the Account!");
            return false;
        }

        lock.l_type = F_WRLCK;//lock type as write lock
        lock.l_start = offset;

        int lockingStat = fcntl(accountFD, F_SETLKW, &lock);//locking and storing file descriptor in lockingStat
        if (lockingStat == -1)//Error in obtaining write lock on the Account file
        {
            perror("Error in obtaining write lock on the Account file!");
            return false;
        }

        writeBytes = write(accountFD, &account, sizeof(struct Account));
        if (writeBytes == -1)//Error in deleting account record
        {
            perror("Error in deleting account record!");
            return false;
        }

        lock.l_type = F_UNLCK;//lock type as unlock
        fcntl(accountFD, F_SETLK, &lock);//unlocking

        strcpy(writeBuff, ADMIN_DEL_ACCOUNT_SUCCESS);
    }
    else
        // Account has some money ask customer to withdraw it
        strcpy(writeBuff, ADMIN_DEL_ACCOUNT_FAILURE);
    writeBytes = write(connFD, writeBuff, strlen(writeBuff));
    if (writeBytes == -1)//Error in writing final DEL message to client
    {
        perror("Error in writing final DEL message to client!");
        return false;
    }
    readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read

    return true;
}

//modify customer function

bool modify_customer_details(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuff[1000], writeBuff[1000];

    struct Customer customer;//customer

    int customerID;//customer ID

    off_t offset;
    int lockingStat;

    writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_ID, strlen(ADMIN_MOD_CUSTOMER_ID));
    if (writeBytes == -1)//Error in writing ADMIN_MOD_CUSTOMER_ID message to client
    {
        perror("Error in writing ADMIN_MOD_CUSTOMER_ID message to client!");
        return false;
    }
    bzero(readBuff, sizeof(readBuff));//empty read buffer
    readBytes = read(connFD, readBuff, sizeof(readBuff));
    if (readBytes == -1)//Error in reading customer ID from client
    {
        perror("Error in reading customer ID from client!");
        return false;
    }

    customerID = atoi(readBuff);

    int customerFD = open(CUSTOMER_FILE, O_RDONLY);//opeing customer file and storing file descriptor
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
    
    offset = lseek(customerFD, customerID * sizeof(struct Customer), SEEK_SET);
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

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};//defining lock

    // Lock the record to be read
    lockingStat = fcntl(customerFD, F_SETLKW, &lock);//locking and storing locking status
     if (lockingStat == -1)//Lock cannot be obtained on customer record
    {
        perror("Lock cannot be obtained on customer record!");
        return false;
    }

    readBytes = read(customerFD, &customer, sizeof(struct Customer));
    if (readBytes == -1)//Error in reading customer record from the file
    {
        perror("Error in reading customer record from the file!");
        return false;
    }

    // Unlock the record
    lock.l_type = F_UNLCK;//lock type ad unlock
    fcntl(customerFD, F_SETLK, &lock);//unlocking

    close(customerFD);//closing customer file descriptor

    writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_MENU, strlen(ADMIN_MOD_CUSTOMER_MENU));
    if (writeBytes == -1)//Error in writing ADMIN_MOD_CUSTOMER_MENU message to client
    {
        perror("Error in writing ADMIN_MOD_CUSTOMER_MENU message to client!");
        return false;
    }
    readBytes = read(connFD, readBuff, sizeof(readBuff));
    if (readBytes == -1)//Error in getting customer modification menu choice from client
    {
        perror("Error in getting customer modification menu choice from client!");
        return false;
    }

    int choice = atoi(readBuff);//choice
    if (choice == 0)
    { // A non-numeric string was passed to atoi
        bzero(writeBuff, sizeof(writeBuff));//empty write buffer
        strcpy(writeBuff, ERRON_INPUT_FOR_NUMBER);
        writeBytes = write(connFD, writeBuff, strlen(writeBuff));
        if (writeBytes == -1)//Error in writing ERRON_INPUT_FOR_NUMBER message to client
        {
            perror("Error in writing ERRON_INPUT_FOR_NUMBER message to client!");
            return false;
        }
        readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
        return false;
    }

    bzero(readBuff, sizeof(readBuff));//empty read buffer
    switch (choice)
    {
    case 1:
        writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_NEW_NAME, strlen(ADMIN_MOD_CUSTOMER_NEW_NAME));
        if (writeBytes == -1)//Error in writing ADMIN_MOD_CUSTOMER_NEW_NAME message to client
        {
            perror("Error in writing ADMIN_MOD_CUSTOMER_NEW_NAME message to client!");
            return false;
        }
        readBytes = read(connFD, &readBuff, sizeof(readBuff));
        if (readBytes == -1)//Error in getting response for customer's new name from client
        {
            perror("Error in getting response for customer's new name from client!");
            return false;
        }
        strcpy(customer.name, readBuff);
        break;
    case 2:
        writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_NEW_AGE, strlen(ADMIN_MOD_CUSTOMER_NEW_AGE));
        if (writeBytes == -1)//Error in writing ADMIN_MOD_CUSTOMER_NEW_AGE message to client
        {
            perror("Error in writing ADMIN_MOD_CUSTOMER_NEW_AGE message to client!");
            return false;
        }
        readBytes = read(connFD, &readBuff, sizeof(readBuff));
        if (readBytes == -1)//Error in getting response for customer's new age from client
        {
            perror("Error in getting response for customer's new age from client!");
            return false;
        }
        int updatedAge = atoi(readBuff);//update age
        if (updatedAge == 0)
        {
            // Either client has sent age as 0 (which is invalid) or has entered a non-numeric string
            bzero(writeBuff, sizeof(writeBuff));//empty write buffer
            strcpy(writeBuff, ERRON_INPUT_FOR_NUMBER);
            writeBytes = write(connFD, writeBuff, strlen(writeBuff));
            if (writeBytes == -1)//Error in writing ERRON_INPUT_FOR_NUMBER message to client
            {
                perror("Error in writing ERRON_INPUT_FOR_NUMBER message to client!");
                return false;
            }
            readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
            return false;
        }
        customer.age = updatedAge;
        break;
    case 3:
        writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_NEW_GENDER, strlen(ADMIN_MOD_CUSTOMER_NEW_GENDER));
        if (writeBytes == -1)//Error in writing ADMIN_MOD_CUSTOMER_NEW_GENDER message to client
        {
            perror("Error in writing ADMIN_MOD_CUSTOMER_NEW_GENDER message to client!");
            return false;
        }
        readBytes = read(connFD, &readBuff, sizeof(readBuff));
        if (readBytes == -1)//Error in getting response for customer's new gender from client
        {
            perror("Error in getting response for customer's new gender from client!");
            return false;
        }
        customer.gender = readBuff[0];//customer gender
        break;
    default:
        bzero(writeBuff, sizeof(writeBuff));//empty write buffer
        strcpy(writeBuff, INVALID_MENU_CHOICE);
        writeBytes = write(connFD, writeBuff, strlen(writeBuff));
        if (writeBytes == -1)//Error in writing INVALID_MENU_CHOICE message to client
        {
            perror("Error in writing INVALID_MENU_CHOICE message to client!");
            return false;
        }
        readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read
        return false;
    }

    customerFD = open(CUSTOMER_FILE, O_WRONLY);//opening customer file and storing customer file descriptor
    if (customerFD == -1)//Error in opening customer file
    {
        perror("Error in opening customer file");
        return false;
    }
    offset = lseek(customerFD, customerID * sizeof(struct Customer), SEEK_SET);
    if (offset == -1)//Error in seeking to required customer record
    {
        perror("Error in seeking to required customer record!");
        return false;
    }

    lock.l_type = F_WRLCK;//lock type as write lock
    lock.l_start = offset;
    lockingStat = fcntl(customerFD, F_SETLKW, &lock);//locking and storing locking status
    if (lockingStat == -1)//Error in obtaining write lock on customer record
    {
        perror("Error in obtaining write lock on customer record!");
        return false;
    }

    writeBytes = write(customerFD, &customer, sizeof(struct Customer));
    if (writeBytes == -1)//Error in writing update customer info into file
    {
        perror("Error in writing update customer info into file");
    }

    lock.l_type = F_UNLCK;//lock type unlock
    fcntl(customerFD, F_SETLKW, &lock);//unlocking

    close(customerFD);//closing file descriptor of customer

    writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_SUCCESS, strlen(ADMIN_MOD_CUSTOMER_SUCCESS));
    if (writeBytes == -1)//Error in writing ADMIN_MOD_CUSTOMER_SUCCESS message to client
    {
        perror("Error in writing ADMIN_MOD_CUSTOMER_SUCCESS message to client!");
        return false;
    }
    readBytes = read(connFD, readBuff, sizeof(readBuff)); // Dummy read

    return true;
}

#endif