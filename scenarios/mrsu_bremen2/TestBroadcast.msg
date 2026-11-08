packet TestBroadcast
{
    string vehicleId;
    double timestamp;
    double posX;
    double posY;
    double speed;
    long packetsSentTillNow;
    long packetsReceivedTillNow;
    long knownVehicleCount;
}