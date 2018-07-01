#include "CanSimulationDataGenerator.h"
#include "CanAnalyzerSettings.h"

CanSimulationDataGenerator::CanSimulationDataGenerator()
{
}

CanSimulationDataGenerator::~CanSimulationDataGenerator()
{
}

void CanSimulationDataGenerator::Initialize( U32 simulation_sample_rate, CanAnalyzerSettings* settings )
{
	mSimulationSampleRateHz = simulation_sample_rate;
	mSettings = settings;

	mClockGenerator.Init( mSettings->mBitRate, simulation_sample_rate );
	mCanSimulationData.SetChannel( mSettings->mCanChannel );
	mCanSimulationData.SetSampleRate( simulation_sample_rate );
	mCanSimulationData.SetInitialBitState( mSettings->Recessive() );

	mCanSimulationData.Advance( mClockGenerator.AdvanceByHalfPeriod( 10.0 ) );  //insert 10 bit-periods of idle

	mValue = 0;
}

U32 CanSimulationDataGenerator::GenerateSimulationData( U64 largest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels )
{
	U64 adjusted_largest_sample_requested = AnalyzerHelpers::AdjustSimulationTargetSample( largest_sample_requested, sample_rate, mSimulationSampleRateHz );

	std::vector<U8> data;
	std::vector<U8> empty_data;

	while( mCanSimulationData.GetCurrentSampleNumber() < adjusted_largest_sample_requested )
	{
		data.clear();
		data.push_back( mValue + 0 );
		data.push_back( mValue + 1 );
		data.push_back( mValue + 2 );
		data.push_back( mValue + 3 );

		data.push_back( mValue + 4 );
		data.push_back( mValue + 5 );
		data.push_back( mValue + 6 );
		data.push_back( mValue + 7 );

		mValue++;


		CreateDataOrRemoteFrame( 123, false, false, data, true );
		WriteFrame();

		CreateDataOrRemoteFrame( 321, true, false, data, true );
		WriteFrame();

		CreateDataOrRemoteFrame( 456, true, false, data, true );
		WriteFrame(true);	

		mCanSimulationData.Advance( mClockGenerator.AdvanceByHalfPeriod( 40 ) );

		CreateDataOrRemoteFrame( 123, false, true, empty_data, true );
		WriteFrame();	

		CreateDataOrRemoteFrame( 321, true, true, empty_data, true );
		WriteFrame();	

		mCanSimulationData.Advance( mClockGenerator.AdvanceByHalfPeriod( 100 ) );
	}

	*simulation_channels = &mCanSimulationData;
	return 1;  // we are retuning the size of the SimulationChannelDescriptor array.  In our case, the "array" is length 1.
}

void CanSimulationDataGenerator::CreateDataOrRemoteFrame( U32 identifier, bool use_extended_frame_format, bool remote_frame, std::vector<U8>& data, bool get_ack_in_response )
{
	//A DATA FRAME is composed of seven different bit fields:
	//START OF FRAME, ARBITRATION FIELD, CONTROL FIELD, DATA FIELD, CRC
	//FIELD, ACK FIELD, END OF FRAME. The DATA FIELD can be of length zero.

	mFakeStuffedBits.clear();
	mFakeFixedFormBits.clear();
	mFakeStartOfFrameField.clear();
	mFakeArbitrationField.clear();
	mFakeControlField.clear();
	mFakeDataField.clear();
	mFakeCrcFieldWithoutDelimiter.clear();
	mFakeAckField.clear();
	mFakeEndOfFrame.clear();

	//START OF FRAME (Standard Format as well as Extended Format)
	//The START OF FRAME (SOF) marks the beginning of DATA FRAMES and REMOTE
	//FRAMEs. It consists of a single dominant bit.
	//A station is only allowed to start transmission when the bus is idle (see INTERFRAME
	//Spacing). All stations have to synchronize to the leading edge caused by START OF
	//FRAME (see HARD SYNCHRONIZATION) of the station starting transmission first.

	mFakeStartOfFrameField.push_back( mSettings->Dominant() );

	//ARBITRATION FIELD
	//The format of the ARBITRATION FIELD is different for Standard Format and
	//Extended Format Frames.

	//In Standard Format the ARBITRATION FIELD consists of the 11 bit IDENTIFIER
	//and the RTR-BIT. The IDENTIFIER bits are denoted ID-28 ... ID-18.

	//In Extended Format the ARBITRATION FIELD consists of the 29 bit IDENTIFIER,
	//the SRR-Bit, the IDE-Bit, and the RTR-BIT. The IDENTIFIER bits are denoted ID-28
	//... ID-0.

	if( use_extended_frame_format == true )
	{
		U32 mask = 1 << 28;  //28 bits in exdended format's identifier.
		
		//IDENTIFIER - Extended Format
		//In contrast to the Standard Format the Extended Format consists of 29 bits. The
		//format comprises two sections:

		//Base ID
		//The Base ID consists of 11 bits. It is transmitted in the order from ID-28 to ID-18. It
		//is equivalent to format of the Standard Identifier. The Base ID defines the Extended
		//Frames base priority.

		//Extended ID
		//The Extended ID consists of 18 bits. It is transmitted in the order of ID-17 to ID-0.

		//11 bits of identifier:
		for( U32 i=0; i<11; i++ )
		{
			if( ( mask & identifier ) == 0 )
				mFakeArbitrationField.push_back( mSettings->Dominant() );
			else
				mFakeArbitrationField.push_back( mSettings->Recessive() );

			mask >>= 1;
		}

		//the next bit is SRR
		//SRR BIT (Extended Format)
		//Substitute Remote Request BIT
		//The SRR is a recessive bit. It is transmitted in Extended Frames at the position of the
		//RTR bit in Standard Frames and so substitutes the RTR-Bit in the Standard Frame.
		//Therefore, collisions of a Standard Frame and an Extended Frame, the Base ID (see
		//Extended IDENTIFIER below) of which is the same as the Standard Frames Identifier,
		//are resolved in such a way that the Standard Frame prevails the Extended Frame.

		mFakeArbitrationField.push_back( mSettings->Recessive() );  //SSR bit

		//the next bit is IDE
		//IDE BIT (Extended Format)
		//Identifier Extension Bit
		//The IDE Bit belongs to
		//- the ARBITRATION FIELD for the Extended Format
		//- the Control Field for the Standard Format
		//The IDE bit in the Standard Format is transmitted dominant, whereas in the Extended
		//Format the IDE bit is recessive.

		mFakeArbitrationField.push_back( mSettings->Recessive() ); //IDE bit

		//18 bits of identifier:
		for( U32 i=0; i<18; i++ )
		{
			if( ( mask & identifier ) == 0 )
				mFakeArbitrationField.push_back( mSettings->Dominant() );
			else
				mFakeArbitrationField.push_back( mSettings->Recessive() );

			mask >>= 1;
		}

		//the next bit is RTR
		//RTR BIT (Standard Format as well as Extended Format)
		//Remote Transmission Request BIT
		//In DATA FRAMEs the RTR BIT has to be dominant. Within a REMOTE FRAME the
		//RTR BIT has to be recessive.
		
		if( remote_frame == true )
			mFakeArbitrationField.push_back( mSettings->Recessive() ); //RTR bit
		else
			mFakeArbitrationField.push_back( mSettings->Dominant() ); //RTR bit


		//next is the control field.  r1, r0, DLC3, DLC2, DLC1, DLC0

		//CONTROL FIELD (Standard Format as well as Extended Format)
		//The CONTROL FIELD consists of six bits. The format of the CONTROL FIELD is
		//different for Standard Format and Extended Format. Frames in Standard Format
		//include the DATA LENGTH CODE, the IDE bit, which is transmitted dominant (see
		//above), and the reserved bit r0. Frames in the Extended Format include the DATA
		//LENGTH CODE and two reserved bits r1 and r0. The reserved bits have to be sent
		//dominant, but receivers accept dominant and recessive bits in all combinations.
		
		mFakeControlField.push_back( mSettings->Recessive() ); //r1 bit
		mFakeControlField.push_back( mSettings->Recessive() ); //r0 bit
	}else
	{
		//IDENTIFIER
		//IDENTIFIER - Standard Format
		//The IDENTIFIERs length is 11 bits and corresponds to the Base ID in Extended
		//Format. These bits are transmitted in the order from ID-28 to ID-18. The least
		//significant bit is ID-18. The 7 most significant bits (ID-28 - ID-22) must not be all
		//recessive.

		//In a Standard Frame the IDENTIFIER is followed by the RTR bit.
		//RTR BIT (Standard Format as well as Extended Format)
		//Remote Transmission Request BIT
		//In DATA FRAMEs the RTR BIT has to be dominant. Within a REMOTE FRAME the
		//RTR BIT has to be recessive.


		U32 mask = 1 << 10;
		for( U32 i=0; i<11; i++ )
		{
			if( ( mask & identifier ) == 0 )
				mFakeArbitrationField.push_back( mSettings->Dominant() );
			else
				mFakeArbitrationField.push_back( mSettings->Recessive() );

			mask >>= 1;
		}

		//the next bit is RTR
		//RTR BIT (Standard Format as well as Extended Format)
		//Remote Transmission Request BIT
		//In DATA FRAMEs the RTR BIT has to be dominant. Within a REMOTE FRAME the
		//RTR BIT has to be recessive.
		
		if( remote_frame == true )
			mFakeArbitrationField.push_back( mSettings->Recessive() ); //RTR bit
		else
			mFakeArbitrationField.push_back( mSettings->Dominant() ); //RTR bit

		//next is the control field.  r1, r0, DLC3, DLC2, DLC1, DLC0

		//CONTROL FIELD (Standard Format as well as Extended Format)
		//The CONTROL FIELD consists of six bits. The format of the CONTROL FIELD is
		//different for Standard Format and Extended Format. Frames in Standard Format
		//include the DATA LENGTH CODE, the IDE bit, which is transmitted dominant (see
		//above), and the reserved bit r0. Frames in the Extended Format include the DATA
		//LENGTH CODE and two reserved bits r1 and r0. The reserved bits have to be sent
		//dominant, but receivers accept dominant and recessive bits in all combinations.
		
		mFakeControlField.push_back( mSettings->Dominant() ); //IDE bit
		mFakeControlField.push_back( mSettings->Dominant() ); //r0 bit
	}


	//send 4 bits for the length of the attached data.
	U32 data_size = data.size();
	if( data_size > 9 )
		AnalyzerHelpers::Assert( "can't sent more than 8 bytes" );

	if( remote_frame == true )
		if( data_size != 0 )
			AnalyzerHelpers::Assert( "remote frames can't send data" );

	U32 mask = 1 << 3;
	for( U32 i=0; i<4; i++ )
	{
		if( ( mask & data_size ) == 0 )
			mFakeControlField.push_back( mSettings->Dominant() );
		else
			mFakeControlField.push_back( mSettings->Recessive() );

		mask >>= 1;
	}

	//next is the DATA FIELD
	//DATA FIELD (Standard Format as well as Extended Format)
	//The DATA FIELD consists of the data to be transferred within a DATA FRAME. It can
	//contain from 0 to 8 bytes, which each contain 8 bits which are transferred MSB first.

	if( remote_frame == false )
	{
		for( U32 i=0; i<data_size; i++ )
		{
			U32 dat = data[i];		
			U32 mask = 0x80;

			for( U32 j=0; j<8; j++ )
			{
				if( ( mask & dat ) == 0 )
					mFakeDataField.push_back( mSettings->Dominant() );
				else
					mFakeDataField.push_back( mSettings->Recessive() );

				mask >>= 1;
			}
		}
	}

	//CRC FIELD (Standard Format as well as Extended Format)
	//contains the CRC SEQUENCE followed by a CRC DELIMITER.

	AddCrc();


	//ACK FIELD (Standard Format as well as Extended Format)
	//The ACK FIELD is two bits long and contains the ACK SLOT and the ACK DELIMITER.
	//In the ACK FIELD the transmitting station sends two recessive bits.
	//A RECEIVER which has received a valid message correctly, reports this to the
	//TRANSMITTER by sending a dominant bit during the ACK SLOT (it sends ACK).

	//ACK SLOT
	//All stations having received the matching CRC SEQUENCE report this within the ACK
	//SLOT by superscribing the recessive bit of the TRANSMITTER by a dominant bit.

	if( get_ack_in_response == true )
		mFakeAckField.push_back( mSettings->Dominant() );
	else
		mFakeAckField.push_back( mSettings->Recessive() );

	//ACK DELIMITER
	//The ACK DELIMITER is the second bit of the ACK FIELD and has to be a recessive
	//bit. As a consequence, the ACK SLOT is surrounded by two recessive bits (CRC
	//DELIMITER, ACK DELIMITER).

	mFakeAckField.push_back( mSettings->Recessive() );

	//END OF FRAME (Standard Format as well as Extended Format)
	//Each DATA FRAME and REMOTE FRAME is delimited by a flag sequence consisting
	//of seven recessive bits.

	for( U32 i=0; i<7; i++ )
		mFakeEndOfFrame.push_back( mSettings->Recessive() );


	mFakeFixedFormBits.insert( mFakeFixedFormBits.end(), mFakeAckField.begin(), mFakeAckField.end() );
	mFakeFixedFormBits.insert( mFakeFixedFormBits.end(), mFakeEndOfFrame.begin(), mFakeEndOfFrame.end() );
}

void CanSimulationDataGenerator::AddCrc()
{	
	mFakeStuffedBits.insert( mFakeStuffedBits.end(), mFakeStartOfFrameField.begin(), mFakeStartOfFrameField.end() );
	mFakeStuffedBits.insert( mFakeStuffedBits.end(), mFakeArbitrationField.begin(), mFakeArbitrationField.end() );
	mFakeStuffedBits.insert( mFakeStuffedBits.end(), mFakeControlField.begin(), mFakeControlField.end() );
	mFakeStuffedBits.insert( mFakeStuffedBits.end(), mFakeDataField.begin(), mFakeDataField.end() );

	U32 bits_for_crc = mFakeStuffedBits.size();
	U16 crc = ComputeCrc( mFakeStuffedBits, bits_for_crc );
	U32 mask = 0x4000;
	for( U32 i=0; i<15; i++ )
	{
		if( ( mask & crc ) == 0 )
			mFakeCrcFieldWithoutDelimiter.push_back( mSettings->Dominant() );
		else
			mFakeCrcFieldWithoutDelimiter.push_back( mSettings->Recessive() );

		mask >>= 1;
	}

	mFakeStuffedBits.insert( mFakeStuffedBits.end(), mFakeCrcFieldWithoutDelimiter.begin(), mFakeCrcFieldWithoutDelimiter.end() );

	//CRC DELIMITER (Standard Format as well as Extended Format)
	//The CRC SEQUENCE is followed by the CRC DELIMITER which consists of a single
	//recessive bit.

	mFakeFixedFormBits.push_back( mSettings->Recessive() );
}

U16 CanSimulationDataGenerator::ComputeCrc( std::vector<BitState>& bits, U32 num_bits )
{
	//note that this is a 15 bit CRC (not 16-bit)

	//CRC FIELD (Standard Format as well as Extended Format)
	//contains the CRC SEQUENCE followed by a CRC DELIMITER.
	//CRC SEQUENCE (Standard Format as well as Extended Format)
	//The frame check sequence is derived from a cyclic redundancy code best suited for
	///frames with bit counts less than 127 bits (BCH Code).
	//In order to carry out the CRC calculation the polynomial to be divided is defined as the
	//polynomial, the coefficients of which are given by the destuffed bit stream consisting of
	//START OF FRAME, ARBITRATION FIELD, CONTROL FIELD, DATA FIELD (if
	///present) and, for the 15 lowest coefficients, by 0. This polynomial is divided (the
	//coefficients are calculated modulo-2) by the generator-polynomial:
	//X15 + X14 + X10 + X8 + X7 + X4 + X3 + 1.
	//The remainder of this polynomial division is the CRC SEQUENCE transmitted over the
	//bus. In order to implement this function, a 15 bit shift register CRC_RG(14:0) can be
	//used. If NXTBIT denotes the next bit of the bit stream, given by the destuffed bit
	//sequence from START OF FRAME until the end of the DATA FIELD, the CRC
	//SEQUENCE is calculated as follows:

	//CRC_RG = 0; // initialize shift register
	//REPEAT
	//CRCNXT = NXTBIT EXOR CRC_RG(14);
	//CRC_RG(14:1) = CRC_RG(13:0); // shift left by
	//CRC_RG(0) = 0; // 1 position
	//IF CRCNXT THEN
	//CRC_RG(14:0) = CRC_RG(14:0) EXOR (4599hex);
	//ENDIF
	//UNTIL (CRC SEQUENCE starts or there is an ERROR condition)

	//After the transmission / reception of the last bit of the DATA FIELD, CRC_RG contains
	//the CRC sequence.

	U16 crc_result = 0;
	for( U32 i=0; i<num_bits; i++ )
	{
		BitState next_bit = bits[i];

		//Exclusive or
		if( ( crc_result & 0x4000 ) != 0  )
		{
			next_bit = Invert( next_bit );  //if the msb of crc_result is zero, then next_bit is not inverted; otherwise, it is.
		}
		
		crc_result <<= 1;

		if( next_bit == mSettings->Recessive() ) //normally bit high.
			crc_result ^= 0x4599;
	}

	return crc_result & 0x7FFF;
}

void CanSimulationDataGenerator::WriteFrame( bool error )
{
	U32 recessive_count = 0;
	U32 dominant_count = 0;

	//The frame segments START OF FRAME, ARBITRATION FIELD, CONTROL FIELD,
	//DATA FIELD and CRC SEQUENCE are coded by the method of bit stuffing. Whenever
	//a transmitter detects five consecutive bits of identical value in the bit stream to be
	//transmitted it automatically inserts a complementary bit in the actual transmitted bit
	//stream.

	//The remaining bit fields of the DATA FRAME or REMOTE FRAME (CRC DELIMITER,
	//ACK FIELD, and END OF FRAME) are of fixed form and not stuffed. The ERROR
	//FRAME and the OVERLOAD FRAME are of fixed form as well and not coded by the
	//method of bit stuffing.

	U32 count = mFakeStuffedBits.size();

	if( error == true )
		count -= 9;

	for( U32 i=0; i<count; i++ )
	{

		if( recessive_count == 5 )
		{
			mCanSimulationData.Advance( mClockGenerator.AdvanceByHalfPeriod( 1.0 ) );
			recessive_count = 0;
			dominant_count = 1; // this stuffed bit counts

			mCanSimulationData.Transition(); //to DOMINANT
		}

		if( dominant_count == 5 )
		{
			mCanSimulationData.Advance( mClockGenerator.AdvanceByHalfPeriod( 1.0 ) );
			dominant_count = 0;
			recessive_count = 1; // this stuffed bit counts

			mCanSimulationData.Transition(); //to RECESSIVE
		}

		BitState bit = mFakeStuffedBits[i];

		if( bit == mSettings->Recessive() )
		{	
			recessive_count++;
			dominant_count = 0;
		}else
		{
			dominant_count++;
			recessive_count = 0;
		}

		mCanSimulationData.Advance( mClockGenerator.AdvanceByHalfPeriod( 1.0 ) );
		mCanSimulationData.TransitionIfNeeded( bit );
	}

	if( error == true )
	{	
		if( mCanSimulationData.GetCurrentBitState() != mSettings->Dominant() )
		{
			mCanSimulationData.Advance( mClockGenerator.AdvanceByHalfPeriod( 1.0 ) );
			mCanSimulationData.Transition(); //to DOMINANT
		}
		
		mCanSimulationData.Advance( mClockGenerator.AdvanceByHalfPeriod( 8.0 ) );

		mCanSimulationData.Transition(); //to DOMINANT

		return;
	}

	count = mFakeFixedFormBits.size();

	for( U32 i=0; i<count; i++ )
	{
		mCanSimulationData.Advance( mClockGenerator.AdvanceByHalfPeriod( 1.0 ) );
		mCanSimulationData.TransitionIfNeeded( mFakeFixedFormBits[i] );
	}
}


