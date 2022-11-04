#ifndef TRANSACTIONS
#define TRANSACTIONS

#include <time.h>
//Transaction structure
struct Transaction
{
    int transactionID; // 0, 1, 2, 3 ...
    int accountNumber;//account number
    bool operation; // 0 for Withdraw, 1 for Deposit
    long int oldBalance; // old balance
    long int newBalance;// new balance
    time_t transactionTime; //transaction time
};

#endif