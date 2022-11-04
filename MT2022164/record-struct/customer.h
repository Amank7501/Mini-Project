#ifndef CUSTOMER_RECORD
#define CUSTOMER_RECORD


//customer structure
struct Customer
{
    int id; // 0, 1, 2 ....
    char name[25];
    char gender; // M for Male, F for Female, O for Other
    int age;
    // Login Credentials
    char login[30]; // Structure : name-id (name will the first word in the structure member `name`)
    char password[30]; //password
    // Bank data
    int account; // Account number of the account the customer owns
};

#endif