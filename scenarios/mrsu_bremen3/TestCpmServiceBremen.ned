import artery.application.ItsG5Service;

simple TestCpmServiceBremen3 like ItsG5Service
{
    parameters:
        double cpmInterval @unit(s) = default(1s);
}
