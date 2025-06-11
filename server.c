/* CONCEPTUAL SERVER ALGORITHM (CONCURRENT CONNECTION-ORIENTED WITH PROCESSES)*/
/* 
    1. Create a socket
    2. Bind the socket to an address and port
    3. Put the socket in passive listening mode
    4. Accept a connection from a client
    5. Fork a new process to handle the client
    6. In the child process:
    REPEAT:
        a. Read data from the client
        b. Parse the command and arguments
        c. Execute the command (e.g., open, close, withdraw) by calling the appropriate function
        d. Prepare the response based on the command execution result
        e. Send the response back to the client
    If the command is "quit", break the loop and close the client socket 
    7. In the parent process:
        a. Close the client socket and continue listening for new connections
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

#include "bank.h"

#define PORT 8080
#define BUFFER_SIZE 1024
#define ACCOUNTS_DATA_FILE "accounts_data.txt"
#define MAX_ARGS 10 // Define maximum number of arguments expected

// Signal handler to reap zombie processes
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Trim leading and trailing whitespace
void trim_whitespace(char *str) {
    char *end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0) // All spaces?
        return;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end+1) = 0;
}

// Convert string to lowercase
void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

// handle a single client connection
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read;

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        // Read from client
        bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0) {
            break; // Connection closed or error
        }
        buffer[bytes_read] = '\0';

        // Trim whitespace from the whole received string first
        trim_whitespace(buffer);

        char command[50] = {0};
        char args[MAX_ARGS][100] = {0};
        int arg_count = 0;
        char response[BUFFER_SIZE * 2] = {0}; // Increased buffer size for potential statement output

        // --- Protocol Parsing: COMMAND,arg1,arg2,...,argN; ---

        // Check for the terminating semicolon
        int len = strlen(buffer);
        if (len == 0 || buffer[len - 1] != ';') {
            snprintf(response, sizeof(response), "ERROR Invalid protocol format: Missing terminating semicolon ';'.\n");
            send(client_socket, response, strlen(response), 0);
            continue; 
        }

        // Remove the semicolon for easier parsing
        buffer[len - 1] = '\0';

        // Use strtok to split the string by commas
        char *token = strtok(buffer, ",");
        int token_index = 0;
        // Parse the command and arguments
        // The first token is the command, the rest are arguments
        while (token != NULL && token_index < MAX_ARGS + 1) {
            trim_whitespace(token); // Trim whitespace from each token
            if (token_index == 0) {
                // First token is the command
                strncpy(command, token, sizeof(command) - 1);
                command[sizeof(command) - 1] = '\0';
                to_lowercase(command); // Commands are case-insensitive
            } else {
                // Subsequent tokens are arguments
                strncpy(args[arg_count], token, sizeof(args[arg_count]) - 1);
                args[arg_count][sizeof(args[arg_count]) - 1] = '\0';
                arg_count++;
            }
            token = strtok(NULL, ",");
            token_index++;
        }

        // --- Command Handling based on parsed tokens ---

        if (strcmp(command, "open") == 0) {
            // Expected format: open,name,national_id,account_type,initial_deposit,pin;
            if (arg_count == 5) {
                char* name = args[0];
                char* national_id = args[1];
                char* account_type = args[2];
                double initial_deposit = atof(args[3]);
                int pin = atoi(args[4]);

                to_lowercase(account_type);

                if (strcmp(account_type, "savings") != 0 && strcmp(account_type, "checking") != 0) {
                     snprintf(response, sizeof(response), "ERROR Invalid account type. Use 'savings' or 'checking'.\n");
                } else {
                    //call function
                    Account new_acc = open_account(name, national_id, account_type, initial_deposit, pin);
                    //formulate the response
                    if (new_acc.is_active) {
                        // Success
                        snprintf(response, sizeof(response), "OK,Account Number:%s,PIN:%d;\n",
                                 new_acc.account_number, new_acc.pin);
                    } else {
                        // Failure (e.g., national ID already exists)
                        snprintf(response, sizeof(response), "ERROR 2 Failed to open account. National ID may already exist or invalid deposit amount.\n");
                    }
                }
            } else {
                snprintf(response, sizeof(response), "ERROR Invalid OPEN command format. Usage: OPEN,name,national_id,account_type,initial_deposit,pin;\n");
            }
        } else if (strcmp(command, "close") == 0) {
            // Expected format: close,account_number,pin;
            if (arg_count == 2) {
                char* acc_num = args[0];
                int pin = atoi(args[1]);

                int result = close_account(acc_num, pin);

                if (result == 0) {
                    snprintf(response, sizeof(response), "OK,Account %s closed successfully.;\n", acc_num);
                } else {
                    snprintf(response, sizeof(response), "ERROR 1 Account not found or incorrect PIN.;\n");
                }
            } else {
                snprintf(response, sizeof(response), "ERROR Invalid CLOSE command format. Usage: CLOSE,account_number,pin;\n");
            }
        } else if (strcmp(command, "withdraw") == 0) {
            // Expected format: withdraw,account_number,pin,amount;
            if (arg_count == 3) {
                char* acc_num = args[0];
                int pin = atoi(args[1]);
                double amount = atof(args[2]);

                int result = withdraw(acc_num, pin, amount);

                if (result == 0) {
                    snprintf(response, sizeof(response), "OK,Withdrawal successful.;\n");
                } else if (result == 1) {
                    snprintf(response, sizeof(response), "ERROR 1 Account not found or incorrect PIN.;\n");
                } else if (result == 3) {
                    snprintf(response, sizeof(response), "ERROR 3 Insufficient funds or minimum balance requirement not met.;\n");
                } else if (result == 4) {
                     snprintf(response, sizeof(response), "ERROR 4 Withdrawal amount must be a positive multiple of 500.;\n");
                }
                 else {
                    snprintf(response, sizeof(response), "ERROR Unknown withdrawal error code: %d.;\n", result);
                 }
            } else {
                snprintf(response, sizeof(response), "ERROR Invalid WITHDRAW command format. Usage: WITHDRAW,account_number,pin,amount;\n");
            }
        } else if (strcmp(command, "deposit") == 0) {
            // Expected format: deposit,account_number,pin,amount;
            if (arg_count == 3) {
                char* acc_num = args[0];
                int pin = atoi(args[1]);
                double amount = atof(args[2]);

                int result = deposit(acc_num, pin, amount);

                if (result == 0) {
                    snprintf(response, sizeof(response), "OK,Deposit successful.;\n");
                } else if (result == 1) {
                    snprintf(response, sizeof(response), "ERROR 1 Account not found or incorrect PIN.;\n");
                } else if (result == 3) {
                     snprintf(response, sizeof(response), "ERROR 3 Minimum deposit amount is 500.;\n");
                }
                 else {
                    snprintf(response, sizeof(response), "ERROR Unknown deposit error code: %d.;\n", result);
                 }
            } else {
                snprintf(response, sizeof(response), "ERROR Invalid DEPOSIT command format. Usage: DEPOSIT,account_number,pin,amount;\n");
            }
        } else if (strcmp(command, "balance") == 0) {
            // Expected format: balance,account_number,pin;
            if (arg_count == 2) {
                char* acc_num = args[0];
                int pin = atoi(args[1]);

                double balance = check_balance(acc_num, pin);

                if (balance >= 0.0) { // check_balance returns -1.0 on error
                    snprintf(response, sizeof(response), "OK,Balance:%.2f;\n", balance);
                } else {
                    snprintf(response, sizeof(response), "ERROR 1 Account not found or incorrect PIN.;\n");
                }
            } else {
                snprintf(response, sizeof(response), "ERROR Invalid BALANCE command format. Usage: BALANCE,account_number,pin;\n");
            }
        } else if (strcmp(command, "statement") == 0) {
            // Expected format: statement,account_number,pin;
            if (arg_count == 2) {
                char* acc_num = args[0];
                int pin = atoi(args[1]);

                char statement_output[BUFFER_SIZE * 2]; // larger buffer for statement
                memset(statement_output, 0, sizeof(statement_output));

                int result = get_statement(acc_num, pin, statement_output, sizeof(statement_output));

                if (result == 0) {
                    // Prepend "OK," to the statement output and append ";"
                    snprintf(response, sizeof(response), "OK,%s;\n", statement_output);
                } else if (result == 1) {
                    snprintf(response, sizeof(response), "ERROR 1 Account not found or incorrect PIN.;\n");
                } else if (result == 2) {
                    snprintf(response, sizeof(response), "ERROR 2 Statement buffer too small.;\n");
                }
                 else {
                    snprintf(response, sizeof(response), "ERROR Unknown statement error code: %d.;\n", result);
                 }
            } else {
                snprintf(response, sizeof(response), "ERROR Invalid STATEMENT command format. Usage: STATEMENT,account_number,pin;\n");
            }
        } else if (strcmp(command, "quit") == 0) {
             if (arg_count == 0) {
                snprintf(response, sizeof(response), "OK,Connection terminated.;\n");
                send(client_socket, response, strlen(response), 0);
                break; // Exit the handling loop
             } else {
                snprintf(response, sizeof(response), "ERROR Invalid QUIT command format. Usage: QUIT;\n");
             }
        } else {
            snprintf(response, sizeof(response), "ERROR Unknown command: %s;\n", command);
        }

        // Send the response back to the client
        send(client_socket, response, strlen(response), 0);
    }

}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    pid_t pid;

    srand(time(NULL)); //seed for pin generation
    //load accounts
    printf("Loading accounts from %s...\n", ACCOUNTS_DATA_FILE);
    load_accounts_from_file(ACCOUNTS_DATA_FILE);
    printf("Loaded %d accounts.\n", account_count);


    // Set up signal handler for SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error in socket creation");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to address and port
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error in binding");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) == 0) {
        printf("Server listening on port %d...\n", PORT);
    } else {
        perror("Error in listening");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Accept a client connection
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
        if (client_socket < 0) {
            perror("Error in accepting connection");
            continue; // Continue accepting other connections
        }

        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Fork a child process to handle the client
        pid = fork();

        if (pid < 0) {
            perror("Error in forking");
            close(client_socket);
            continue;
        }

        if (pid == 0) { // Child process
            close(server_socket); // Close the listening socket in the child
            handle_client(client_socket); // Handle the client communication
            exit(EXIT_SUCCESS); // Then exit
        } else { // Parent process
            close(client_socket); // Close the client socket in the parent
            // Parent continues to listen for new connections
        }
    }

    // Typically unreachable in a server that runs indefinitely
    printf("shutting down...\n");
    save_accounts_to_file(ACCOUNTS_DATA_FILE); 
    printf("Accounts saved.\n");
    close(server_socket);

    return 0;
}
