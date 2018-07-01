#ifndef CAN_ANALYZER_RESULTS
#define CAN_ANALYZER_RESULTS

#include <AnalyzerResults.h>

enum CanFrameType { IdentifierField, IdentifierFieldEx, ControlField, DataField, CrcField, AckField, CanError };
#define REMOTE_FRAME ( 1 << 0 )

//#define FRAMING_ERROR_FLAG ( 1 << 0 )
//#define PARITY_ERROR_FLAG ( 1 << 1 )

class CanAnalyzer;
class CanAnalyzerSettings;

class CanAnalyzerResults : public AnalyzerResults
{
public:
	CanAnalyzerResults( CanAnalyzer* analyzer, CanAnalyzerSettings* settings );
	virtual ~CanAnalyzerResults();

	virtual void GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base );
	virtual void GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id );

	virtual void GenerateFrameTabularText(U64 frame_index, DisplayBase display_base );
	virtual void GeneratePacketTabularText( U64 packet_id, DisplayBase display_base );
	virtual void GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base );

protected: //functions

protected:  //vars
	CanAnalyzerSettings* mSettings;
	CanAnalyzer* mAnalyzer;
};

#endif //CAN_ANALYZER_RESULTS
