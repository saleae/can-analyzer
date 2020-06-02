#ifndef CAN_ANALYZER_SETTINGS
#define CAN_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

//#define RECESSIVE BIT_HIGH
//#define DOMINANT BIT_LOW

class CanAnalyzerSettings : public AnalyzerSettings
{
  public:
    CanAnalyzerSettings();
    virtual ~CanAnalyzerSettings();

    virtual bool SetSettingsFromInterfaces();
    virtual void LoadSettings( const char* settings );
    virtual const char* SaveSettings();

    void UpdateInterfacesFromSettings();

    Channel mCanChannel;
    U32 mBitRate;
    bool mInverted;

    BitState Recessive();
    BitState Dominant();

  protected:
    std::auto_ptr<AnalyzerSettingInterfaceChannel> mCanChannelInterface;
    std::auto_ptr<AnalyzerSettingInterfaceInteger> mBitRateInterface;
    std::auto_ptr<AnalyzerSettingInterfaceBool> mCanChannelInvertedInterface;
};
#endif // CAN_ANALYZER_SETTINGS
