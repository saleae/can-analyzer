
#include "CanAnalyzer.h"
#include "CanAnalyzerSettings.h"
#include <AnalyzerChannelData.h>


CanAnalyzer::CanAnalyzer() : Analyzer2(), mSettings( new CanAnalyzerSettings() ), mSimulationInitilized( false )
{
    SetAnalyzerSettings( mSettings.get() );
    UseFrameV2();
}

CanAnalyzer::~CanAnalyzer()
{
    KillThread();
}

void CanAnalyzer::SetupResults()
{
    mResults.reset( new CanAnalyzerResults( this, mSettings.get() ) );
    SetAnalyzerResults( mResults.get() );
    mResults->AddChannelBubblesWillAppearOn( mSettings->mCanChannel );
}

void CanAnalyzer::WorkerThread()
{
    mSampleRateHz = GetSampleRate();
    mCan = GetAnalyzerChannelData( mSettings->mCanChannel );

    InitSampleOffsets();
    WaitFor7RecessiveBits(); // first of all, let's get at least 7 recessive bits in a row, to make sure we're in-between frames.

    // now let's pull in the frames, one at a time.
    for( ;; )
    {
        if( mCan->GetBitState() == mSettings->Recessive() )
            mCan->AdvanceToNextEdge();

        // we're at the first DOMINANT edge of the frame
        GetRawFrame();
        AnalizeRawFrame();

        if( mCanError == true )
        {
            FrameV2 frame_v2_error;
            Frame frame;
            frame.mStartingSampleInclusive = mErrorStartingSample;
            frame.mEndingSampleInclusive = mErrorEndingSample;
            frame.mType = CanError;
            mResults->AddFrame( frame );
            mResults->AddFrameV2( frame_v2_error, "can_error", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
            mResults->CancelPacketAndStartNewPacket();
        }

        U32 count = mCanMarkers.size();
        for( U32 i = 0; i < count; i++ )
        {
            if( mCanMarkers[ i ].mType == Standard )
                mResults->AddMarker( mCanMarkers[ i ].mSample, AnalyzerResults::Dot, mSettings->mCanChannel );
            else
                mResults->AddMarker( mCanMarkers[ i ].mSample, AnalyzerResults::ErrorX, mSettings->mCanChannel );
        }

        mResults->CommitResults();
        ReportProgress( mCan->GetSampleNumber() );
        CheckIfThreadShouldExit();

        if( mCanError == true )
        {
            WaitFor7RecessiveBits();
        }
    }
}

void CanAnalyzer::InitSampleOffsets()
{
    mSampleOffsets.resize( 256 );

    double samples_per_bit = double( mSampleRateHz ) / double( mSettings->mBitRate );
    double samples_behind = 0.0;

    U32 increment = U32( ( samples_per_bit * .5 ) + samples_behind );
    samples_behind = ( samples_per_bit * .5 ) + samples_behind - double( increment );

    mSampleOffsets[ 0 ] = increment;
    U32 current_offset = increment;

    for( U32 i = 1; i < 256; i++ )
    {
        U32 increment = U32( samples_per_bit + samples_behind );
        samples_behind = samples_per_bit + samples_behind - double( increment );
        current_offset += increment;
        mSampleOffsets[ i ] = current_offset;
    }

    mNumSamplesIn7Bits = U32( samples_per_bit * 7.0 );
}

void CanAnalyzer::WaitFor7RecessiveBits()
{
    if( mCan->GetBitState() == mSettings->Dominant() )
        mCan->AdvanceToNextEdge();

    for( ;; )
    {
        if( mCan->WouldAdvancingCauseTransition( mNumSamplesIn7Bits ) == false )
            return;

        mCan->AdvanceToNextEdge();
        mCan->AdvanceToNextEdge();
    }
}

void CanAnalyzer::GetRawFrame()
{
    mCanError = false;
    mRecessiveCount = 0;
    mDominantCount = 0;
    mRawBitResults.clear();

    if( mCan->GetBitState() != mSettings->Dominant() )
        AnalyzerHelpers::Assert( "GetFrameOrError assumes we start DOMINANT" );

    mStartOfFrame = mCan->GetSampleNumber();

    U32 i = 0;
    // what we're going to do now is capture a sequence up until we get 7 recessive bits in a row.
    for( ;; )
    {
        if( i > 255 )
        {
            // we are in garbage data most likely, lets get out of here.
            return;
        }

        mCan->AdvanceToAbsPosition( mStartOfFrame + mSampleOffsets[ i ] );
        i++;

        if( mCan->GetBitState() == mSettings->Dominant() )
        {
            // the bit is DOMINANT
            mDominantCount++;
            mRecessiveCount = 0;
            mRawBitResults.push_back( mSettings->Dominant() );

            if( mDominantCount == 6 )
            {
                // we have detected an error.

                mCanError = true;
                mErrorStartingSample = mStartOfFrame + mSampleOffsets[ i - 5 ];
                mErrorEndingSample = mStartOfFrame + mSampleOffsets[ i ];

                // don't use any of these error bits in analysis.
                mRawBitResults.pop_back();
                mRawBitResults.pop_back();
                mRawBitResults.pop_back();
                mRawBitResults.pop_back();
                mRawBitResults.pop_back();
                mRawBitResults.pop_back();

                mNumRawBits = mRawBitResults.size();

                // the channel is currently high.  addvance it to the next start bit.
                // no, don't bother, we want to analyze this packet before we advance.

                break;
            }
        }
        else
        {
            // the bit is RECESSIVE
            mRecessiveCount++;
            mDominantCount = 0;
            mRawBitResults.push_back( mSettings->Recessive() );

            if( mRecessiveCount == 7 )
            {
                // we're done.
                break;
            }
        }
    }

    mNumRawBits = mRawBitResults.size();
}


void CanAnalyzer::AnalizeRawFrame()
{
    BitState bit;
    U64 last_sample;

    UnstuffRawFrameBit( bit, last_sample, true ); // grab the start bit, and reset everything.
    mArbitrationField.clear();
    mControlField.clear();
    mDataField.clear();
    mCrcFieldWithoutDelimiter.clear();
    mAckField.clear();

    bool done;

    mIdentifier = 0;
    for( U32 i = 0; i < 11; i++ )
    {
        mIdentifier <<= 1;
        BitState bit;
        done = UnstuffRawFrameBit( bit, last_sample );
        if( done == true )
            return;
        mArbitrationField.push_back( bit );

        if( bit == mSettings->Recessive() )
            mIdentifier |= 1;
    }

    // ok, the next three bits will let us know if this is 11-bit or 29-bit can.  If it's 11-bit, then it'll also tell us if this is a
    // remote frame request or not.

    BitState bit0;
    done = UnstuffRawFrameBit( bit0, last_sample );
    if( done == true )
        return;

    BitState bit1;
    done = UnstuffRawFrameBit( bit1, last_sample );
    if( done == true )
        return;

    // ok, if bit1 is dominant, then this is 11-bit.

    Frame frame;
    FrameV2 frame_v2_identifier;


    if( bit1 == mSettings->Dominant() )
    {
        // 11-bit CAN

        BitState bit2; // since this is 11-bit CAN, we know that bit2 is the r0 bit, which we are going to throw away.
        done = UnstuffRawFrameBit( bit2, last_sample );
        if( done == true )
            return;

        mStandardCan = true;

        frame.mStartingSampleInclusive = mStartOfFrame + mSampleOffsets[ 1 ];
        frame.mEndingSampleInclusive = last_sample;
        frame.mType = IdentifierField;

        if( bit0 == mSettings->Recessive() ) // since this is 11-bit CAN, we know that bit0 is the RTR bit
        {
            mRemoteFrame = true;
            frame.mFlags = REMOTE_FRAME;
            frame_v2_identifier.AddBoolean( "remote_frame", true );
        }
        else
        {
            mRemoteFrame = false;
            frame.mFlags = 0;
        }

        frame.mData1 = mIdentifier;
        frame_v2_identifier.AddInteger( "identifier", mIdentifier );
        mResults->AddFrame( frame );
        mResults->AddFrameV2( frame_v2_identifier, "identifier_field", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
    }
    else
    {
        // 29-bit CAN

        mStandardCan = false;

        // get the next 18 address bits.
        for( U32 i = 0; i < 18; i++ )
        {
            mIdentifier <<= 1;

            BitState bit;
            done = UnstuffRawFrameBit( bit, last_sample );
            if( done == true )
                return;
            mArbitrationField.push_back( bit );

            if( bit == mSettings->Recessive() )
                mIdentifier |= 1;
        }

        // get the RTR bit
        BitState rtr;
        done = UnstuffRawFrameBit( rtr, last_sample );
        if( done == true )
            return;

        // get the r0 and r1 bits (we won't use them)
        BitState r0;
        done = UnstuffRawFrameBit( r0, last_sample );
        if( done == true )
            return;

        BitState r1;
        done = UnstuffRawFrameBit( r1, last_sample );
        if( done == true )
            return;

        Frame frame;
        frame.mStartingSampleInclusive = mStartOfFrame + mSampleOffsets[ 1 ];
        frame.mEndingSampleInclusive = last_sample;
        frame.mType = IdentifierFieldEx;

        if( rtr == mSettings->Recessive() )
        {
            mRemoteFrame = true;
            frame.mFlags = REMOTE_FRAME;
            frame_v2_identifier.AddBoolean( "RemoteFrame", true );
        }
        else
        {
            mRemoteFrame = false;
            frame.mFlags = 0;
        }

        frame.mData1 = mIdentifier;
        frame_v2_identifier.AddInteger( "identifier", mIdentifier );
        frame_v2_identifier.AddBoolean( "extended", true );
        mResults->AddFrame( frame );
        mResults->AddFrameV2( frame_v2_identifier, "identifier_field", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
    }


    U32 mask = 0x8;
    mNumDataBytes = 0;
    U64 first_sample = 0;
    for( U32 i = 0; i < 4; i++ )
    {
        BitState bit;
        if( i == 0 )
            done = UnstuffRawFrameBit( bit, first_sample );
        else
            done = UnstuffRawFrameBit( bit, last_sample );

        if( done == true )
            return;

        mControlField.push_back( bit );

        if( bit == mSettings->Recessive() )
            mNumDataBytes |= mask;

        mask >>= 1;
    }
    FrameV2 frame_v2_control;

    frame.mStartingSampleInclusive = first_sample;
    frame.mEndingSampleInclusive = last_sample;
    frame.mType = ControlField;
    frame.mData1 = mNumDataBytes;
    mResults->AddFrame( frame );
    frame_v2_control.AddInteger( "num_data_bytes", mNumDataBytes );
    mResults->AddFrameV2( frame_v2_control, "control_field", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );

    U32 num_bytes = mNumDataBytes;
    if( num_bytes > 8 )
        num_bytes = 8;

    if( mRemoteFrame == true )
        num_bytes = 0; // ignore the num_bytes if this is a remote frame.

    for( U32 i = 0; i < num_bytes; i++ )
    {
        U32 data = 0;
        U32 mask = 0x80;
        for( U32 j = 0; j < 8; j++ )
        {
            BitState bit;

            if( j == 0 )
                done = UnstuffRawFrameBit( bit, first_sample );
            else
                done = UnstuffRawFrameBit( bit, last_sample );

            if( done == true )
                return;

            if( bit == mSettings->Recessive() )
                data |= mask;

            mask >>= 1;

            mDataField.push_back( bit );
        }
        FrameV2 frame_v2_data;
        frame.mStartingSampleInclusive = first_sample;
        frame.mEndingSampleInclusive = last_sample;
        frame.mType = DataField;
        frame.mData1 = data;
        mResults->AddFrame( frame );
        frame_v2_data.AddByte( "data", data );
        mResults->AddFrameV2( frame_v2_data, "data_field", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
    }

    mCrcValue = 0;
    for( U32 i = 0; i < 15; i++ )
    {
        mCrcValue <<= 1;
        BitState bit;

        if( i == 0 )
            done = UnstuffRawFrameBit( bit, first_sample );
        else
            done = UnstuffRawFrameBit( bit, last_sample );

        if( done == true )
            return;

        mCrcFieldWithoutDelimiter.push_back( bit );

        if( bit == mSettings->Recessive() )
            mCrcValue |= 1;
    }
    FrameV2 frame_v2_crc;
    frame.mStartingSampleInclusive = first_sample;
    frame.mEndingSampleInclusive = last_sample;
    frame.mType = CrcField;
    frame.mData1 = mCrcValue;
    mResults->AddFrame( frame );
    frame_v2_crc.AddInteger( "crc", mCrcValue );
    mResults->AddFrameV2( frame_v2_crc, "crc_field", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );

    done = UnstuffRawFrameBit( mCrcDelimiter, first_sample );

    if( done == true )
        return;

    BitState ack;
    done = GetFixedFormFrameBit( ack, first_sample );

    mAckField.push_back( ack );
    if( ack == mSettings->Dominant() )
        mAck = true;
    else
        mAck = false;

    done = GetFixedFormFrameBit( ack, last_sample );

    if( done == true )
        return;

    mAckField.push_back( ack );

    FrameV2 frame_v2_ack;
    frame.mStartingSampleInclusive = first_sample;
    frame.mEndingSampleInclusive = last_sample;
    frame.mType = AckField;
    frame.mData1 = mAck;
    mResults->AddFrame( frame );
    frame_v2_ack.AddBoolean( "ack", mAck );
    mResults->AddFrameV2( frame_v2_ack, "ack_field", frame.mStartingSampleInclusive, frame.mEndingSampleInclusive );
    mResults->CommitPacketAndStartNewPacket();
}

bool CanAnalyzer::GetFixedFormFrameBit( BitState& result, U64& sample )
{
    if( mNumRawBits == mRawFrameIndex )
        return true;

    result = mRawBitResults[ mRawFrameIndex ];
    sample = mStartOfFrame + mSampleOffsets[ mRawFrameIndex ];
    mCanMarkers.push_back( CanMarker( sample, Standard ) );
    mRawFrameIndex++;

    return false;
}

bool CanAnalyzer::UnstuffRawFrameBit( BitState& result, U64& sample, bool reset )
{
    if( reset == true )
    {
        mRecessiveCount = 0;
        mDominantCount = 0;
        mRawFrameIndex = 0;
        mCanMarkers.clear();
    }

    if( mRawFrameIndex == mNumRawBits )
        return true;

    if( mRecessiveCount == 5 )
    {
        mRecessiveCount = 0;
        mDominantCount = 1; // this bit is DOMINANT, and counts twards the next bit stuff
        mCanMarkers.push_back( CanMarker( mStartOfFrame + mSampleOffsets[ mRawFrameIndex ], BitStuff ) );
        mRawFrameIndex++;
    }

    if( mDominantCount == 5 )
    {
        mDominantCount = 0;
        mRecessiveCount = 1; // this bit is RECESSIVE, and counts twards the next bit stuff
        mCanMarkers.push_back( CanMarker( mStartOfFrame + mSampleOffsets[ mRawFrameIndex ], BitStuff ) );
        mRawFrameIndex++;
    }

    if( mRawFrameIndex == mNumRawBits )
        return true;

    result = mRawBitResults[ mRawFrameIndex ];

    if( result == mSettings->Recessive() )
    {
        mRecessiveCount++;
        mDominantCount = 0;
    }
    else
    {
        mDominantCount++;
        mRecessiveCount = 0;
    }

    sample = mStartOfFrame + mSampleOffsets[ mRawFrameIndex ];
    mCanMarkers.push_back( CanMarker( sample, Standard ) );
    mRawFrameIndex++;

    return false;
}


bool CanAnalyzer::NeedsRerun()
{
    return false;
}

U32 CanAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate,
                                         SimulationChannelDescriptor** simulation_channels )
{
    if( mSimulationInitilized == false )
    {
        mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), mSettings.get() );
        mSimulationInitilized = true;
    }

    return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index, device_sample_rate, simulation_channels );
}

U32 CanAnalyzer::GetMinimumSampleRateHz()
{
    return mSettings->mBitRate * 8;
}

const char* CanAnalyzer::GetAnalyzerName() const
{
    return "CAN";
}

const char* GetAnalyzerName()
{
    return "CAN";
}

Analyzer* CreateAnalyzer()
{
    return new CanAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
    delete analyzer;
}
