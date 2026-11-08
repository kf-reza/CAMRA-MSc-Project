import traci.ModuleMapper;

simple MrsuModuleMapper3 like traci.ModuleMapper
{
    parameters:
        @class(traci::MrsuModuleMapper3);
        string personType;
        string vehicleType;
        double penetrationRate = default(1.0);
        double personPenetrationRate = default(-1.0);
        double vehiclePenetrationRate = default(-1.0);
}