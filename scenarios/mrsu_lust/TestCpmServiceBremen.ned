import artery.application.ItsG5Service;

simple TestCpmServiceLuST like ItsG5Service
{
    parameters:
        double cpmInterval @unit(s) = default(1s);
}
