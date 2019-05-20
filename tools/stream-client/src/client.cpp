#include <sys/types.h>
#include <signal.h>
#include <iostream>
#include <string>
#include <system_error>
#include <functional>

#include <asio.hpp>
#include <chrono>
#include "rpsa/server/core/AsioNet.h"
#include "rpsa/server/core/StreamingManager.h"


using namespace std;

CStreamingManager::Ptr                g_manger;
asionet::CAsioNet::Ptr                g_asionet;
bool                                  g_terminate;
uint64_t                              g_BytesCount;
long long int                         g_timeBegin;
uint64_t                              g_lostRate;
uint64_t                              g_packCounter_ch1;
uint64_t                              g_packCounter_ch2;

char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
    //    Example
    //    char * filename = getCmdOption(argv, argv + argc, "-f");

    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    //    Example
    //    if(cmdOptionExists(argv, argv+argc, "-h"))
    //    {  // Do stuff    }
    return std::find(begin, end, option) != end;
}

bool CheckMissing(const char* val,const char* Message)
{
    if (val == NULL) {
        std::cout << "Missing parameters: " << Message << std::endl;
        return true;
    }
    return false;
}

string ParseIpAddress(string value) noexcept {
    try {
        size_t colonPos = value.find(':');

        if(colonPos != std::string::npos)
        {
            return value.substr(0,colonPos);
        }
        return  value;
    }
    catch (std::exception& e)
    {
        std::cout << "Error read ip address from parameters";
        exit(3);
    }
}

string ParsePort(string value) noexcept{
    try {
         size_t colonPos = value.find(':');

         if (colonPos != std::string::npos) {
             string port = value.substr(colonPos + 1);
             if (port == "")
                 port = "8900";
             return port;
         }
         return "8900";
    }
    catch (std::exception& e)
    {
        std::cout << "Error read port number from parameters";
        return "8900";
    }
}


void UsingArgs(char const* progName){
    std::cout << "Usage: " << progName << "\n";
    std::cout << "\t-h IP_ADDRESS:[port] (default value 8900)\n";
    std::cout << "\t-p Protocol (TCP or UDP required value)\n";
    std::cout << "\t-f Path to the directory where to save files\n";
    std::cout << "\t-t Type of file (tdms or wav required value)\n";


}

std::string time_point_to_string(std::chrono::system_clock::time_point &tp)
{
    using namespace std;
    using namespace std::chrono;

    auto ttime_t = system_clock::to_time_t(tp);
    auto tp_sec = system_clock::from_time_t(ttime_t);
    milliseconds ms = duration_cast<milliseconds>(tp - tp_sec);

    std::tm * ttm = localtime(&ttime_t);

    char date_time_format[] = "%Y.%m.%d-%H.%M.%S";

    char time_str[] = "yyyy.mm.dd.HH-MM.SS.fff";

    strftime(time_str, strlen(time_str), date_time_format, ttm);

    string result(time_str);
    result.append(".");
    result.append(to_string(ms.count()));

    return result;
}

void reciveData(std::error_code error,uint8_t *buff,size_t _size){
     //std::cout << "Get data: " <<  _size << "\n";
     g_BytesCount += _size;
     uint8_t *ch1 = nullptr;
     uint8_t *ch2 = nullptr;
     size_t   size_ch1 = 0;
     size_t   size_ch2 = 0;
     uint64_t id = 0;
     uint64_t lostRate = 0;
     uint32_t oscRate = 0;
     uint32_t resolution = 0;
     asionet::CAsioNet::ExtractPack(buff,_size, id, lostRate,oscRate, resolution, ch1, size_ch1, ch2 , size_ch2);

     g_packCounter_ch1 += size_ch1 / (resolution == 16 ? 2 : 1);
     g_packCounter_ch2 += size_ch2 / (resolution == 16 ? 2 : 1);
     g_lostRate += lostRate;


     g_manger->passBuffers(lostRate, oscRate, ch1 , size_ch1 ,  ch2 , size_ch2 , resolution, id);


//     std::cout << id << " ; " <<  _size  <<  " ; " << resolution << " ; " << size_ch1 << " ; " << size_ch2 << "\n";

     delete [] ch1;
     delete [] ch2;

     std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
     auto curTime = std::chrono::time_point_cast<std::chrono::milliseconds >(timeNow);
     auto value = curTime.time_since_epoch();
//     std::cout << value.count() << "\n";
//     std::cout <<  g_timeBegin << "\n";
     if ((value.count() - g_timeBegin) >= 5000) {

         std::cout << time_point_to_string(timeNow) << " bandwidth: " << g_BytesCount / (1024 * 1024 * 5) << " MiB/s;\nData count ch1:\t" << g_packCounter_ch1
                 << " ch2:\t" << g_packCounter_ch2 <<  " Lost: \t"<< g_lostRate << "\n\n";
         g_BytesCount = 0;
         g_lostRate = 0;
         g_timeBegin = value.count();
     }



}


void sigHandler (int sigNum){
    g_manger->stop();
    g_asionet->Stop();
    g_terminate = true;
}


int main(int argc, char* argv[])
{
//    try
//    {

        std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
        auto curTime = std::chrono::time_point_cast<std::chrono::milliseconds >(timeNow);
        auto value = curTime.time_since_epoch();
        g_timeBegin = value.count();

        g_packCounter_ch1 = 0;
        g_packCounter_ch2 = 0;
        g_lostRate = 0;
        g_BytesCount = 0;
        g_terminate = false;
        signal(SIGINT, sigHandler);
      //  signal(SIGKILL, sigHandler);
        asionet::Protocol protocol_val;

        char * filepath  = getCmdOption(argv, argv + argc, "-f");
        char * ip_port   = getCmdOption(argv, argv + argc, "-h");
        char * protocol  = getCmdOption(argv, argv + argc, "-p");
        char * type_file = getCmdOption(argv, argv + argc, "-t");
        bool checkParameters = false;
        checkParameters |= CheckMissing(ip_port,"IP address of server");
        checkParameters |= CheckMissing(protocol,"Protocol");
        checkParameters |= CheckMissing(type_file,"Type of file");
        if (checkParameters) {
            UsingArgs(argv[0]);
            return -1;
        }

        string host = ParseIpAddress(ip_port);
        string port = ParsePort(ip_port);
        if (filepath == nullptr)
            filepath = const_cast<char *>(".");

#ifndef  _WIN32
        auto size =  FileQueueManager::GetFreeSpaceDisk(filepath);
        std::cout << "Free disk space: "<< size / (1024 * 1024) << "Mb \n";
#endif

        if (strcmp(protocol,"TCP")==0){
            protocol_val = asionet::Protocol::TCP;
        }else if (strcmp(protocol,"UDP")==0){
            protocol_val = asionet::Protocol::UDP;
        }else{
            std::cout << "Error: Protocol value has wrong format\n";
            UsingArgs(argv[0]);
            return -1;
        }



        g_manger = CStreamingManager::Create((strcmp(type_file,"wav") == 0 ?
                                              Stream_FileType::WAV_TYPE : Stream_FileType::TDMS_TYPE)  , filepath);

        g_manger->run();

        g_asionet = asionet::CAsioNet::Create(asionet::Mode::CLIENT, protocol_val ,host , port);
        g_asionet->addCallClient_Connect([](std::string host) { std::cout << "Try connect " << host << '\n'; });
        g_asionet->addCallClient_Error([](std::error_code error)
                                       {
                                           std::cout << "Disconnect;" << '\n';
                                           sigHandler(0);
                                       });
        g_asionet->addCallReceived(reciveData);
        g_asionet->Start();
        while(g_manger->isFileThreadWork() &&  !g_terminate){
#ifdef _WIN32
			Sleep(1000);			
#else
			sleep(1);
#endif // _WIN32
            
        }
        sigHandler(0);
//    }
//    catch (std::exception& e)
//   {
//        std::cerr << "Exception: " << e.what() << "\n";
//    }

    return 0;
}
