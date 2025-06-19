#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define IP_ADDRESS "127.0.0.1"
#define PORT 54321
#define PACKET_SIZE 512
#define MAX_PACKETS 1000

typedef struct {
    int SequenceNumber;
    int Length;
    char Data[PACKET_SIZE];
} Packet;

int main() {
    int ServerSocketFileDescriptor;
    struct sockaddr_in ServerAddress, ClientAddress;
    char Buffer[PACKET_SIZE + sizeof(int) * 2];
    FILE *FilePointer = NULL;
    int *ReceivedSequence = NULL;
    int TotalPackets = 0;
    int MaximumSequence = -1;
    long CurrentFileOffset = 0; 

    if ((ServerSocketFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&ServerAddress, 0, sizeof(ServerAddress));
    ServerAddress.sin_family = AF_INET;
    ServerAddress.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    ServerAddress.sin_port = htons(PORT);

    if (bind(ServerSocketFileDescriptor, (struct sockaddr*)&ServerAddress, sizeof(ServerAddress)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Server started, listening on %s:%d\n", IP_ADDRESS, PORT);

    while (1) {
        socklen_t Length = sizeof(ClientAddress);
        int ReceivedBytes = recvfrom(ServerSocketFileDescriptor, Buffer, sizeof(Buffer), 0,
                                     (struct sockaddr*)&ClientAddress, &Length);
        if (ReceivedBytes < 0) {
            perror("Recvfrom failed");
            continue;
        }
        Buffer[ReceivedBytes] = '\0';
        printf("Received %d bytes from %s:%d\n", ReceivedBytes, inet_ntoa(ClientAddress.sin_addr), ntohs(ClientAddress.sin_port));

        // Handle file request
        if (strncmp(Buffer, "FILE_REQUEST", 12) == 0) {
            if (FilePointer) fclose(FilePointer);
            FilePointer = fopen("output_file.txt", "wb");
            if (!FilePointer) {
                perror("File open failed");
                continue;
            }
            if (ReceivedSequence) free(ReceivedSequence);
            ReceivedSequence = calloc(MAX_PACKETS, sizeof(int));
            TotalPackets = 0;
            MaximumSequence = -1;
            CurrentFileOffset = 0; // Reset file offset
            printf("Received file request: %s\n", Buffer);
            sendto(ServerSocketFileDescriptor, "ACK", 3, 0, (struct sockaddr*)&ClientAddress, Length);
            printf("Sent ACK\n");
            continue;
        }

        // Handle end message
        if (strncmp(Buffer, "END:", 4) == 0) {
            sscanf(Buffer, "END:%d", &TotalPackets);
            printf("Received END message, total packets: %d, max_seq: %d\n", TotalPackets, MaximumSequence);

            char Response[PACKET_SIZE];
            int MissingPacketsCount = 0;
            for (int Index = 0; Index < TotalPackets; Index++) {
                if (!ReceivedSequence[Index]) MissingPacketsCount++;
            }

            sprintf(Response, "MISSING:%d:", MissingPacketsCount);
            char *Pointer = Response + strlen(Response);
            for (int Index = 0; Index < TotalPackets; Index++) {
                if (!ReceivedSequence[Index]) {
                    Pointer += sprintf(Pointer, "%d,", Index);
                }
            }
            if (MissingPacketsCount > 0) Pointer[-1] = '\0'; 

            printf("Sending missing packet info: %s\n", Response);
            sendto(ServerSocketFileDescriptor, Response, strlen(Response), 0,
                   (struct sockaddr*)&ClientAddress, Length);
            if (FilePointer) {
                fflush(FilePointer); 
                fclose(FilePointer);
                FilePointer = NULL;
            }
            continue;
        }

        // Handle data packets
        Packet PacketInstance;
        memcpy(&PacketInstance, Buffer, ReceivedBytes);

        printf("Received packet %d, length: %d\n", PacketInstance.SequenceNumber, PacketInstance.Length);

        if (PacketInstance.SequenceNumber > MaximumSequence) MaximumSequence = PacketInstance.SequenceNumber;

        if (FilePointer && PacketInstance.Length > 0) {
           
            fseek(FilePointer, CurrentFileOffset, SEEK_SET);
            //fseek(FilePointer, PacketInstance.SequenceNumber * PACKET_SIZE , SEEK_SET);
            fwrite(PacketInstance.Data, 1, PacketInstance.Length, FilePointer);
            CurrentFileOffset += PacketInstance.Length; 
            if (!ReceivedSequence) {
                ReceivedSequence = calloc(MAX_PACKETS, sizeof(int));
            }
            if (PacketInstance.SequenceNumber < MAX_PACKETS) {
                ReceivedSequence[PacketInstance.SequenceNumber] = 1;
            }
        }
    }

    if (FilePointer) fclose(FilePointer);
    if (ReceivedSequence) free(ReceivedSequence);
    close(ServerSocketFileDescriptor);
    return EXIT_SUCCESS;
}
