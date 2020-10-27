#include <fstream>
#include <ctime>
#include "calib_man.h"


void PrintLogInFile3(const char *message){
	std::time_t result = std::time(nullptr);
	std::fstream fs;
  	fs.open ("/tmp/debug.log", std::fstream::in | std::fstream::out | std::fstream::app);
	fs << std::asctime(std::localtime(&result)) << " : " << message << "\n";
	fs.close();
}

float calibFullScaleToVoltageMan(uint32_t fullScaleGain) {
    if (fullScaleGain == 0) return 1;
    return (float) ((float)fullScaleGain  * 100.0 / ((uint64_t)1<<32));
}

uint32_t calibFullScaleFromVoltageMan(float voltageScale) {
    return (uint32_t) (voltageScale / 100.0 * ((uint64_t)1<<32));
}


CCalibMan::Ptr CCalibMan::Create(COscilloscope::Ptr _acq)
{
    return std::make_shared<CCalibMan>(_acq);
}

CCalibMan::CCalibMan(COscilloscope::Ptr _acq):
m_acq(_acq)
{
    m_currentGain = RP_LOW;
#ifdef Z20_250_12
    m_currentAC_DC = RP_DC;
    m_currentGenGain = RP_GAIN_1X;
#endif
    readCalib();
}

CCalibMan::~CCalibMan()
{

}

void CCalibMan::init(){
    m_acq->resetGen();
    m_currentGain = RP_LOW;
    m_acq->setLV();
#ifdef Z20_250_12
    m_currentAC_DC = RP_DC;
    m_acq->setDC();
    m_currentGenGain = RP_GAIN_1X;
    m_acq->setGenGainx1();
#endif
    readCalib();
}

int CCalibMan::readCalib(){
    m_calib_parameters = rp_GetCalibrationSettings();
    return 0;
}

int CCalibMan::readCalibEpprom(){
    rp_CalibInit();
    m_calib_parameters = rp_GetCalibrationSettings();
    return 0;
}

void CCalibMan::updateCalib(){
    rp_CalibrationSetParams(m_calib_parameters);
}

void CCalibMan::writeCalib(){
    readCalib();
    rp_CalibrationWriteParams(m_calib_parameters);
}

void CCalibMan::setModeLV_HV(rp_pinState_t _mode){
    m_currentGain = _mode;
    if (m_currentGain == RP_LOW){
        m_acq->setLV();
    }
    if (m_currentGain == RP_HIGH){
        m_acq->setHV();
    }
}

rp_pinState_t CCalibMan::getModeLV_HV(){
    return m_currentGain;
}

#ifdef Z20_250_12
void CCalibMan::setModeAC_DC(rp_acq_ac_dc_mode_t _mode){
    m_currentAC_DC = _mode;
    if (m_currentAC_DC == RP_DC){
        m_acq->setDC();
    }
    if (m_currentAC_DC == RP_AC){
        m_acq->setAC();
    }
}

rp_acq_ac_dc_mode_t CCalibMan::getModeAC_DC(){
    return m_currentAC_DC;
}

void CCalibMan::setGenGain(rp_gen_gain_t _mode){
    m_currentGenGain = _mode;
    if (m_currentGenGain == RP_GAIN_1X){
        m_acq->setGenGainx1();
    }
    if (m_currentGenGain == RP_GAIN_5X){
        m_acq->setGenGainx5();
    }
}

rp_gen_gain_t CCalibMan::getGenGain(){
    return m_currentGenGain; 
}           
#endif

int CCalibMan::getCalibValue(ClalibValue _type){
    auto g = getModeLV_HV();
#ifdef Z20_250_12
    auto ac_dc = getModeAC_DC();
    auto gen_g = getGenGain();
    switch (_type)
    {
        case ADC_CH1_OFF:  {
            if (g == RP_LOW)  return  (ac_dc == RP_DC) ? m_calib_parameters.osc_ch1_off_1_dc  : m_calib_parameters.osc_ch1_off_1_ac;
            if (g == RP_HIGH) return  (ac_dc == RP_DC) ? m_calib_parameters.osc_ch1_off_20_dc : m_calib_parameters.osc_ch1_off_20_ac;
            break;
        }
        
        case ADC_CH2_OFF:  {
            if (g == RP_LOW)  return  (ac_dc == RP_DC) ? m_calib_parameters.osc_ch2_off_1_dc  : m_calib_parameters.osc_ch2_off_1_ac;
            if (g == RP_HIGH) return  (ac_dc == RP_DC) ? m_calib_parameters.osc_ch2_off_20_dc : m_calib_parameters.osc_ch2_off_20_ac;
            break;
        }
        
        case ADC_CH1_GAIN: {
            if (g == RP_LOW)  return  (ac_dc == RP_DC) ? m_calib_parameters.osc_ch1_g_1_dc  : m_calib_parameters.osc_ch1_g_1_ac;
            if (g == RP_HIGH) return  (ac_dc == RP_DC) ? m_calib_parameters.osc_ch1_g_20_dc : m_calib_parameters.osc_ch1_g_20_ac;
            break;
        }

        case ADC_CH2_GAIN: {
            if (g == RP_LOW)  return  (ac_dc == RP_DC) ? m_calib_parameters.osc_ch2_g_1_dc  : m_calib_parameters.osc_ch2_g_1_ac;
            if (g == RP_HIGH) return  (ac_dc == RP_DC) ? m_calib_parameters.osc_ch2_g_20_dc : m_calib_parameters.osc_ch2_g_20_ac;
            break;
        }

        case DAC_CH1_OFF:  return  (gen_g == RP_GAIN_1X) ? m_calib_parameters.gen_ch1_off_1  : m_calib_parameters.gen_ch1_off_5;
        case DAC_CH2_OFF:  return  (gen_g == RP_GAIN_1X) ? m_calib_parameters.gen_ch2_off_1  : m_calib_parameters.gen_ch2_off_5;
        case DAC_CH1_GAIN:  return  (gen_g == RP_GAIN_1X) ? m_calib_parameters.gen_ch1_g_1  : m_calib_parameters.gen_ch1_g_5;
        case DAC_CH2_GAIN:  return  (gen_g == RP_GAIN_1X) ? m_calib_parameters.gen_ch2_g_1  : m_calib_parameters.gen_ch2_g_5;
    }
#endif

#ifdef Z10

    switch (_type)
    {
        case ADC_CH1_OFF:  {
            if (g == RP_LOW)  return m_calib_parameters.fe_ch1_lo_offs;
            if (g == RP_HIGH) return m_calib_parameters.fe_ch1_hi_offs;
            break;
        }
        
        case ADC_CH2_OFF:  {
            if (g == RP_LOW)  return m_calib_parameters.fe_ch2_lo_offs;
            if (g == RP_HIGH) return m_calib_parameters.fe_ch2_hi_offs;
            break;
        }
        
        case ADC_CH1_GAIN: {
            if (g == RP_LOW)  return m_calib_parameters.fe_ch1_fs_g_lo;
            if (g == RP_HIGH) return m_calib_parameters.fe_ch1_fs_g_hi;
            break;
        }

        case ADC_CH2_GAIN: {
            if (g == RP_LOW)  return m_calib_parameters.fe_ch2_fs_g_lo;
            if (g == RP_HIGH) return m_calib_parameters.fe_ch2_fs_g_hi;
            break;
        }
       
        case DAC_CH1_OFF:  return m_calib_parameters.be_ch1_dc_offs;
        case DAC_CH2_OFF:  return m_calib_parameters.be_ch2_dc_offs;
        case DAC_CH1_GAIN:  return m_calib_parameters.be_ch1_fs;
        case DAC_CH2_GAIN:  return m_calib_parameters.be_ch2_fs;  
    }
#endif
    return 0;
}

int setCalibInt(int32_t *_x, int _value){
    *_x = _value;
    return 0;
}

int setCalibUInt(uint32_t *_x, int _value){
    *_x = _value;
    return 0;
}

int CCalibMan::setCalibValue(ClalibValue _type, int _value){
auto g = getModeLV_HV();
#ifdef Z20_250_12
    auto ac_dc = getModeAC_DC();
    auto gen_g = getGenGain();
    switch (_type)
    {
        case ADC_CH1_OFF:  {
            if (g == RP_LOW)  return  (ac_dc == RP_DC) ? setCalibInt(&m_calib_parameters.osc_ch1_off_1_dc,_value)  : setCalibInt(&m_calib_parameters.osc_ch1_off_1_ac,_value);
            if (g == RP_HIGH) return  (ac_dc == RP_DC) ? setCalibInt(&m_calib_parameters.osc_ch1_off_20_dc,_value) : setCalibInt(&m_calib_parameters.osc_ch1_off_20_ac,_value);
            break;
        }
        
        case ADC_CH2_OFF:  {
            if (g == RP_LOW)  return  (ac_dc == RP_DC) ? setCalibInt(&m_calib_parameters.osc_ch2_off_1_dc,_value)  : setCalibInt(&m_calib_parameters.osc_ch2_off_1_ac ,_value);
            if (g == RP_HIGH) return  (ac_dc == RP_DC) ? setCalibInt(&m_calib_parameters.osc_ch2_off_20_dc,_value) : setCalibInt(&m_calib_parameters.osc_ch2_off_20_ac,_value);
            break;
        }
        
        case ADC_CH1_GAIN: {
            if (g == RP_LOW)  return  (ac_dc == RP_DC) ? setCalibUInt(&m_calib_parameters.osc_ch1_g_1_dc,_value)  : setCalibUInt(&m_calib_parameters.osc_ch1_g_1_ac,_value);
            if (g == RP_HIGH) return  (ac_dc == RP_DC) ? setCalibUInt(&m_calib_parameters.osc_ch1_g_20_dc,_value) : setCalibUInt(&m_calib_parameters.osc_ch1_g_20_ac,_value);
            break;
        }

        case ADC_CH2_GAIN: {
            if (g == RP_LOW)  return  (ac_dc == RP_DC) ? setCalibUInt(&m_calib_parameters.osc_ch2_g_1_dc,_value)  : setCalibUInt(&m_calib_parameters.osc_ch2_g_1_ac,_value);
            if (g == RP_HIGH) return  (ac_dc == RP_DC) ? setCalibUInt(&m_calib_parameters.osc_ch2_g_20_dc,_value) : setCalibUInt(&m_calib_parameters.osc_ch2_g_20_ac,_value);
            break;
        }

        case DAC_CH1_OFF:   return  (gen_g == RP_GAIN_1X) ? setCalibInt(&m_calib_parameters.gen_ch1_off_1,_value)  : setCalibInt(&m_calib_parameters.gen_ch1_off_5,_value);
        case DAC_CH2_OFF:   return  (gen_g == RP_GAIN_1X) ? setCalibInt(&m_calib_parameters.gen_ch2_off_1,_value)  : setCalibInt(&m_calib_parameters.gen_ch2_off_5,_value);
        case DAC_CH1_GAIN:  return  (gen_g == RP_GAIN_1X) ? setCalibUInt(&m_calib_parameters.gen_ch1_g_1,_value)  : setCalibUInt(&m_calib_parameters.gen_ch1_g_5,_value);
        case DAC_CH2_GAIN:  return  (gen_g == RP_GAIN_1X) ? setCalibUInt(&m_calib_parameters.gen_ch2_g_1,_value)  : setCalibUInt(&m_calib_parameters.gen_ch2_g_5,_value);
    }
#endif

#ifdef Z10

    switch (_type)
    {
        case ADC_CH1_OFF:  {
            if (g == RP_LOW)  return setCalibInt(&m_calib_parameters.fe_ch1_lo_offs,_value);
            if (g == RP_HIGH) return setCalibInt(&m_calib_parameters.fe_ch1_hi_offs,_value);
            break;
        }
        
        case ADC_CH2_OFF:  {
            if (g == RP_LOW)  return setCalibInt(&m_calib_parameters.fe_ch2_lo_offs,_value);
            if (g == RP_HIGH) return setCalibInt(&m_calib_parameters.fe_ch2_hi_offs,_value);
            break;
        }
        
        case ADC_CH1_GAIN: {
            if (g == RP_LOW)  return setCalibUInt(&m_calib_parameters.fe_ch1_fs_g_lo,_value);
            if (g == RP_HIGH) return setCalibUInt(&m_calib_parameters.fe_ch1_fs_g_hi,_value);
            break;
        }

        case ADC_CH2_GAIN: {
            if (g == RP_LOW)  return setCalibUInt(&m_calib_parameters.fe_ch2_fs_g_lo,_value);
            if (g == RP_HIGH) return setCalibUInt(&m_calib_parameters.fe_ch2_fs_g_hi,_value);
            break;
        }
       
        case DAC_CH1_OFF:  return setCalibInt(&m_calib_parameters.be_ch1_dc_offs,_value);
        case DAC_CH2_OFF:  return setCalibInt(&m_calib_parameters.be_ch2_dc_offs,_value);
        case DAC_CH1_GAIN:  return setCalibUInt(&m_calib_parameters.be_ch1_fs,_value);
        case DAC_CH2_GAIN:  return setCalibUInt(&m_calib_parameters.be_ch2_fs,_value);  
    }
#endif
    return -1;
}

int CCalibMan::enableGen(rp_channel_t _ch,bool _enable){
    m_acq->enableGen(_ch,_enable);
}


int CCalibMan::setFreq(rp_channel_t _ch,int _freq){
    m_acq->setFreq(_ch,_freq);
}

int CCalibMan::setAmp(rp_channel_t _ch,float _ampl){
    m_acq->setAmp(_ch,_ampl);
}

int CCalibMan::setOffset(rp_channel_t _ch,float _offset){
    m_acq->setOffset(_ch,_offset);
}

int CCalibMan::setGenType(rp_channel_t _ch,int _type){
    m_acq->setGenType(_ch,_type);
}

void CCalibMan::updateGen(){
    m_acq->updateGenCalib();
}