/**
 * $Id: $
 *
 * @brief Red Pitaya streaming server implementation
 *
 * @Author Red Pitaya
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/prctl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>

#include "rp.h"
#include "StreamingApplication.h"
#include "StreamingManager.h"

#define DEBUG

#ifdef DEBUG
#define RP_LOG(...) \
syslog(__VA_ARGS__);
#else
#define RP_LOG(...)
#endif

std::mutex mtx;
std::condition_variable cv;

float calibFullScaleToVoltage(uint32_t fullScaleGain) {
    /* no scale */
    if (fullScaleGain == 0) {
        return 1;
    }
    return (float) ((float)fullScaleGain  * 100.0 / ((uint64_t)1<<32));
}

char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
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

void UsingArgs(char const* progName){
    std::cout << "Usage: " << progName << "\n";
    std::cout << "\t-b run service in background\n";
    std::cout << "\t-c path to config file\n";
}

CStreamingApplication *s_app = nullptr; 
void StopNonBlocking(int x);

static void handleCloseChildEvents()
{
    struct sigaction sigchld_action; 
    sigchld_action.sa_handler = SIG_DFL,
    sigchld_action.sa_flags = SA_NOCLDWAIT;
 
    sigaction(SIGCHLD, &sigchld_action, NULL);
}


static void termSignalHandler(int signum)
{
    fprintf(stdout,"Received terminate signal. Exiting...\n");
    syslog (LOG_NOTICE, "Received terminate signal. Exiting...");
    StopNonBlocking(0);
}


static void installTermSignalHandler()
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = termSignalHandler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
}

int main(int argc, char *argv[])
{

    char  *filepath  = getCmdOption(argv, argv + argc, "-c");
    bool   is_fork   = cmdOptionExists(argv, argv + argc, "-b");
    bool checkParameters = false;
    checkParameters |= CheckMissing(filepath,"Configuration file");
    if (checkParameters) {
        UsingArgs(argv[0]);
        exit(1);
    }

     // Open logging into "/var/log/messages" or /var/log/syslog" or other configured...
    setlogmask (LOG_UPTO (LOG_INFO));
    openlog ("streaming-server", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

#ifndef Z20
    rp_CalibInit();
    auto osc_calib_params = rp_GetCalibrationSettings();
#endif

    int32_t ch1_off = 0;
    int32_t ch2_off = 0;
    float ch1_gain = 1;
    float ch2_gain = 1;
  


    if (is_fork){
        FILE *fp= NULL;
        pid_t process_id = 0;
        pid_t sid = 0;
        // Create child process
        process_id = fork();
        // Indication of fork() failure
        if (process_id < 0)
        {
            fprintf(stderr,"fork failed!\n");
            // Return failure in exit status
            exit(1);
        }

        // PARENT PROCESS. Need to kill it.
        if (process_id > 0)
        {
            //printf("process_id of child process %d \n", process_id);
            // return success in exit status
            exit(0);
        }

        //unmask the file mode
        umask(0);
        //set new session
        sid = setsid();
        if(sid < 0)
        {
            // Return failure
            exit(1);
        }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
 
    int resolution = 1;
    int channel = 3;
    int protocol = 1;
    int sock_port = 8900;
    std::string ip_addr_host = "127.0.0.1";
    int format = 0;
    int rate = 1;
    bool use_file = false;
    int samples = -1;
    int save_mode = 1;
    bool use_calib = false;
    int attenuator = 0;

    try{
        ifstream file(filepath);
        std::string key;
        std::string value;

	if (!file.good()) throw std::exception();

        while (file >> key >> value) {
            if ("host" == key) {
                ip_addr_host = value;
                continue;
            }
            if ("port" == key) {
                sock_port = stoi(value);
                continue;
            }
            if ("protocol" == key) {
                protocol = stoi(value);
                continue;
            }
            if ("rate" == key) {
                rate = stoi(value);
                continue;
            }
            if ("resolution" == key) {
                resolution = stoi(value);
                continue;
            }
            if ("use_file" == key) {
                use_file = (bool)stoi(value);
                continue;
            }
            if ("format" == key) {
                format = stoi(value);
                continue;
            }
            if ("samples" == key) {
                samples = stoi(value);
                continue;
            }
            if ("channels" == key) {
                channel = stoi(value);
                continue;
            }
            if ("save_mode" == key) {
                save_mode = stoi(value);
                continue;
            }
#ifndef Z20
            if ("attenuator" == key) {
                attenuator = stoi(value);
                continue;
            }
            if ("use_calib" == key) {
                use_calib = (bool)(stoi(value) - 1);
                continue;
            }
#endif
            throw std::exception();
        }
    }catch(std::exception& e)
    {
        fprintf(stderr,  "Error loading configuration file\n");
        RP_LOG (LOG_ERR, "Error loading configuration file");
        exit(1);
    }


    fprintf(stdout,"streaming-server started\n");
    RP_LOG (LOG_NOTICE, "streaming-server started");

    installTermSignalHandler();
    // Handle close child events
    handleCloseChildEvents();

    try {
		CStreamingManager::MakeEmptyDir(FILE_PATH);
	}catch (std::exception& e)
	{
        fprintf(stderr,  "Error: Can't create %s dir %s",FILE_PATH,e.what());
        RP_LOG (LOG_ERR, "Error: Can't create %s dir %s",FILE_PATH,e.what());
	}

    try{

    if (use_calib == 2) {
    #ifdef Z20_250_12
        if (attenuator == 0) {
            ch1_gain = calibFullScaleToVoltage(osc_calib_params.osc_ch1_g_1_dc);  // 1:1
            ch2_gain = calibFullScaleToVoltage(osc_calib_params.osc_ch2_g_1_dc);  // 1:1
            ch1_off  = osc_calib_params.osc_ch1_off_1_dc; 
            ch2_off  = osc_calib_params.osc_ch1_off_2_dc; 
        }else{
            ch1_gain = calibFullScaleToVoltage(osc_calib_params.fe_ch1_fs_g_lo);  // 1:20
            ch2_gain = calibFullScaleToVoltage(osc_calib_params.fe_ch2_fs_g_lo);  // 1:20
            ch1_off  = osc_calib_params.osc_ch1_off_20_dc; 
            ch2_off  = osc_calib_params.osc_ch2_off_20_dc; 		
        }
    #endif

    #ifdef Z10
        if (attenuator == 0) {
            ch1_gain = calibFullScaleToVoltage(osc_calib_params.fe_ch1_fs_g_hi);  // 1:1
            ch2_gain = calibFullScaleToVoltage(osc_calib_params.fe_ch2_fs_g_hi);  // 1:1
            ch1_off  = osc_calib_params.fe_ch1_hi_offs; 
            ch2_off  = osc_calib_params.fe_ch2_hi_offs; 
        }else{
            ch1_gain = calibFullScaleToVoltage(osc_calib_params.fe_ch1_fs_g_lo);  // 1:20
            ch2_gain = calibFullScaleToVoltage(osc_calib_params.fe_ch2_fs_g_lo);  // 1:20
            ch1_off  = osc_calib_params.fe_ch1_lo_offs; 
            ch2_off  = osc_calib_params.fe_ch2_lo_offs; 		
        }
    #endif    
    }

#ifdef Z20_250_12
        rp_AcqSetGain(RP_CH_1, attenuator == 0 ?  RP_LOW : RP_HIGH);
        rp_AcqSetGain(RP_CH_2, attenuator == 0 ?  RP_LOW : RP_HIGH);

        rp_AcqSetAC_DC(RP_CH_1,RP_DC);
        rp_AcqSetAC_DC(RP_CH_2,RP_DC);
#endif


        std::vector<UioT> uioList = GetUioList();
        // Search oscilloscope
        COscilloscope::Ptr osc = nullptr;
        CStreamingManager::Ptr s_manger = nullptr;

        for (const UioT &uio : uioList)
        {
            if (uio.nodeName == "rp_oscilloscope")
            {
                // TODO start server;
                osc = COscilloscope::Create(uio, (channel ==1 || channel == 3) , (channel ==2 || channel == 3) , rate);
                osc->setCalibration(ch1_off,ch1_gain,ch2_off,ch2_gain);
                break;
            }
        }


        if (use_file == false) {
            s_manger = CStreamingManager::Create(
                    ip_addr_host,
                    std::to_string(sock_port).c_str(),
                    protocol == 1 ? asionet::Protocol::TCP : asionet::Protocol::UDP);
        }else{
            s_manger = CStreamingManager::Create((format == 0 ? Stream_FileType::WAV_TYPE: Stream_FileType::TDMS_TYPE) , FILE_PATH , samples, save_mode == 2);
            s_manger->notifyStop = [](int status)
                                {
                                    StopNonBlocking(0);
                                };
        }
        int resolution_val = (resolution == 1 ? 8 : 16);
        s_app = new CStreamingApplication(s_manger, osc, resolution_val, rate, channel, attenuator , ADC_BITS);
        s_app->run();
        delete s_app;
    }catch (std::exception& e)
    {
        fprintf(stderr, "Error: main() %s\n",e.what());
        RP_LOG(LOG_ERR, "Error: main() %s\n",e.what());
    }
    fprintf(stdout,  "streaming-server stopped.\n");
    RP_LOG(LOG_INFO, "streaming-server stopped.");
    closelog ();
    return (EXIT_SUCCESS);
}

void StopServer(int x){
	try{
		if (s_app!= nullptr){
			s_app->stop(false);
		}
	}catch (std::exception& e){
		fprintf(stderr, "Error: StopServer() %s\n",e.what());
        RP_LOG(LOG_ERR, "Error: StopServer() %s\n",e.what());
	}
}

void StopNonBlocking(int x){
	try{
		std::thread th(StopServer ,x);
		th.detach();
	}catch (std::exception& e){
		fprintf(stderr, "Error: StopNonBlocking() %s\n",e.what());
        RP_LOG(LOG_ERR, "Error: StopNonBlocking() %s\n",e.what());
	}
}


