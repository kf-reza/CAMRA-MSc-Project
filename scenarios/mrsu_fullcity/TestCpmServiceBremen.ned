import artery.application.ItsG5Service;

simple TestCpmServiceFC like ItsG5Service
{
    parameters:
        double cpmInterval @unit(s) = default(1s);
}
