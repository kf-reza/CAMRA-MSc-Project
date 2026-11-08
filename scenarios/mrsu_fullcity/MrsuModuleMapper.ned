import traci.ModuleMapper;

simple MrsuModuleMapperFC like traci.ModuleMapper
{
    parameters:
        @class(traci::MrsuModuleMapperFC);
        string personType;
        string vehicleType;
        double penetrationRate = default(1.0);
        double personPenetrationRate = default(-1.0);
        double vehiclePenetrationRate = default(-1.0);
}