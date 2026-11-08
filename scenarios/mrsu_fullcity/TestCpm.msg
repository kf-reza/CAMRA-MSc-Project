cplusplus {{
#include <vector>
#include <omnetpp/simtime.h>
}}

packet TestCpm
{
    int sourceStation;
    double timestamp;
    int objectCount;
    string objectIds;
}
