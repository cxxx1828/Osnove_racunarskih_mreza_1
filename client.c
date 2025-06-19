#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define IP_ADDRESS "127.0.0.1"
#define PORT 54321
#define PACKET_SIZE 512

typedef struct {
    int SequenceNumber;
    int Length;
    char Data[PACKET_SIZE];
} Packet;

int main() {
    int ClientSocketFileDescriptor;
    struct sockaddr_in ServerAddress;
    Packet PacketInstance;
    char Buffer[PACKET_SIZE];
    FILE *FilePointer;

    if ((ClientSocketFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct timeval TimeoutValue;
    TimeoutValue.tv_sec = 5;
    TimeoutValue.tv_usec = 0;
    setsockopt(ClientSocketFileDescriptor, SOL_SOCKET, SO_RCVTIMEO, &TimeoutValue, sizeof(TimeoutValue));

    memset(&ServerAddress, 0, sizeof(ServerAddress));
    ServerAddress.sin_family = AF_INET;
    ServerAddress.sin_port = htons(PORT);
    ServerAddress.sin_addr.s_addr = inet_addr(IP_ADDRESS);

    char *Request = "FILE_REQUEST:test.txt";
    printf("Sending request: %s\n", Request);
    sendto(ClientSocketFileDescriptor, Request, strlen(Request), 0,
           (struct sockaddr*)&ServerAddress, sizeof(ServerAddress));

    socklen_t Length = sizeof(ServerAddress);
    int ReceivedBytes = recvfrom(ClientSocketFileDescriptor, Buffer, PACKET_SIZE, 0,
                                 (struct sockaddr*)&ServerAddress, &Length);
    if (ReceivedBytes < 0) {
        perror("ACK receive failed");
        close(ClientSocketFileDescriptor);
        exit(EXIT_FAILURE);
    }
    Buffer[ReceivedBytes] = '\0';
    printf("Received: %s\n", Buffer);
    if (strncmp(Buffer, "ACK", 3) != 0) {
        printf("Server rejected request\n");
        close(ClientSocketFileDescriptor);
        return EXIT_FAILURE;
    }

    FilePointer = fopen("test.txt", "rb");
    if (!FilePointer) {
        perror("File open failed");
        close(ClientSocketFileDescriptor);
        exit(EXIT_FAILURE);
    }

    int SequenceNumber = 0;
    int TotalPackets = 0;

    while (1) {
        PacketInstance.SequenceNumber = SequenceNumber+1;
        PacketInstance.Length = fread(PacketInstance.Data, 1, PACKET_SIZE, FilePointer);
        if (PacketInstance.Length <= 0) break;

        sendto(ClientSocketFileDescriptor, &PacketInstance, sizeof(int) * 2 + PacketInstance.Length, 0,
               (struct sockaddr*)&ServerAddress, sizeof(ServerAddress));
        printf("Sent packet %d, length: %d\n", SequenceNumber, PacketInstance.Length);
        SequenceNumber++;
        TotalPackets++;
    }

    char EndMessage[PACKET_SIZE];
    sprintf(EndMessage, "END:%d", TotalPackets);
    printf("Sending end message: %s\n", EndMessage);
    sendto(ClientSocketFileDescriptor, EndMessage, strlen(EndMessage), 0,
           (struct sockaddr*)&ServerAddress, sizeof(ServerAddress));

    ReceivedBytes = recvfrom(ClientSocketFileDescriptor, Buffer, PACKET_SIZE, 0,
                             (struct sockaddr*)&ServerAddress, &Length);
    if (ReceivedBytes < 0) {
        perror("Missing packets info receive failed");
        fclose(FilePointer);
        close(ClientSocketFileDescriptor);
        exit(EXIT_FAILURE);
    }
    Buffer[ReceivedBytes] = '\0';
    printf("Received missing info: %s\n", Buffer);

    int MissingPacketsCount;
    sscanf(Buffer, "MISSING:%d:", &MissingPacketsCount);
    int *MissingSequenceNumbers = malloc(MissingPacketsCount * sizeof(int));
    char *Token = strtok(Buffer + strlen("MISSING:") + 2, ",");
    for (int Index = 0; Index < MissingPacketsCount && Token; Index++) {
        MissingSequenceNumbers[Index] = atoi(Token);
        Token = strtok(NULL, ",");
    }

    fseek(FilePointer, 0, SEEK_SET);
    SequenceNumber = 0;
    while (SequenceNumber < TotalPackets) {
        PacketInstance.SequenceNumber = SequenceNumber;
        PacketInstance.Length = fread(PacketInstance.Data, 1, PACKET_SIZE, FilePointer);

        for (int Index = 0; Index < MissingPacketsCount; Index++) {
            if (SequenceNumber == MissingSequenceNumbers[Index]) {
                sendto(ClientSocketFileDescriptor, &PacketInstance, sizeof(int) * 2 + PacketInstance.Length, 0,
                       (struct sockaddr*)&ServerAddress, sizeof(ServerAddress));
                printf("Resent packet %d\n", SequenceNumber);
            }
        }
        SequenceNumber++;
    }

    fclose(FilePointer);
    free(MissingSequenceNumbers);
    close(ClientSocketFileDescriptor);
    return EXIT_SUCCESS;
}
