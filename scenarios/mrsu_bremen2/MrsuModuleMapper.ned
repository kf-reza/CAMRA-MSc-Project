import traci.ModuleMapper;

simple MrsuModuleMapper like traci.ModuleMapper
{
    parameters:
        @class(traci::MrsuModuleMapper);
        string personType;
        string vehicleType;
        double penetrationRate = default(1.0);
        double personPenetrationRate = default(-1.0);
        double vehiclePenetrationRate = default(-1.0);
}