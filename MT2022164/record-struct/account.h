#ifndef ACCOUNT_RECORD
#define ACCOUNT_RECORD

#define MAX_TRANSACTIONS 10
 
// Account structure 
struct Account
{
    int accountNumber;     // 0, 1, 2, ....
    int owners[2];         // Customer IDs
    bool isRegularAccount; // 1 for Regular account, 0 for Joint account
    bool active;           // 1 for Active, 0 for Deactivated (Deleted)
    long int balance;      // Amount of money in the account
    int transactions[MAX_TRANSACTIONS];  // A list of transaction IDs. Used to look up the transactions. // -1 indicates unused space in array
};

#endif