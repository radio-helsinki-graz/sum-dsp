#include "AxumSummingProcessing.h"

void InitializeProcessing()
{
	int cntChannel;
	int cntInputChannel;
	int cntOutputChannel;
	float *Temp = (float *)Update_MatrixFactor;

	for (cntChannel=0; cntChannel<NUMBEROFFACTORS; cntChannel++)
	{
		MatrixFactor[cntChannel] = 0;
		Temp[cntChannel] = 1;
	}

	Temp = (float *)Update_BussMatrixFactor;
	for (cntOutputChannel = 0; cntOutputChannel < NUMBEROFBUSSMATRIXOUTPUTCHANNELS; cntOutputChannel++)
	{
		for (cntInputChannel = 0; cntInputChannel < NUMBEROFBUSSMATRIXINPUTCHANNELS; cntInputChannel++)
		{	
 			Temp[cntOutputChannel+cntInputChannel*NUMBEROFBUSSMATRIXOUTPUTCHANNELS] = 0;
			if ((cntInputChannel&0x01) == (cntOutputChannel&0x01))
			{
				Temp[cntOutputChannel+cntInputChannel*NUMBEROFBUSSMATRIXOUTPUTCHANNELS] = 1;
			}
		}
	}

	for (cntChannel = 0; cntChannel <((NUMBEROFBUSSMATRIXOUTPUTCHANNELS/2)+(NUMBEROFBUSSMATRIXOUTPUTCHANNELS/2)); cntChannel++)
	{
		PhaseRMS[cntChannel] = 0.01;
	}

	for (cntChannel = 0; cntChannel < NUMBEROFBUSSMATRIXINPUTCHANNELS; cntChannel++)
	{
		SelectedMixMinusBuss[cntChannel] = cntChannel&1;
	}
//	VUReleaseFactor = 0.9;
//	VUReleaseFactor = 0.97671133;
	VUReleaseFactor = 0.999;

	cntMixMinusDelayTop = 3;
	cntMixMinusDelayBottom = 0;
}

void Processing(signed int *InputBuffer, signed int *OutputBuffer)
{
	{
		int cntChannel;

		for (cntChannel=0; cntChannel<64; cntChannel++)
		{
			BussMatrixInput[cntChannel] = InputBuffer[cntChannel];
		}

/* 		for (cntChannel=0; cntChannel<32; cntChannel++)
		{
			MonitorMatrixInput[cntChannel] = InputBuffer[64+cntChannel];
		}*/

/*		for (cntChannel=0; cntChannel<16; cntChannel++)
		{
			BussMatrixInput[cntChannel] = InputBuffer[cntChannel];
		}
		for (cntChannel=0; cntChannel<16; cntChannel++)
		{
			BussMatrixInput[32+cntChannel] = InputBuffer[16+cntChannel];
		}*/
 		for (cntChannel=0; cntChannel<48; cntChannel++)
		{
			MonitorMatrixInput[cntChannel] = InputBuffer[64+cntChannel];
		}
	}

	{
		const float FactorX = SmoothFactor;
		const float FactorY = 1-SmoothFactor;
		int cntChannel;

		StartChannel += NUMBEROFFACTORS/FACTORINTERPOLATIONDIVIDE;
		if (StartChannel>= NUMBEROFFACTORS)
		{
			StartChannel = 0;
		}

		for (cntChannel = 0; cntChannel < NUMBEROFFACTORS/FACTORINTERPOLATIONDIVIDE; cntChannel++)
		{
			MatrixFactor[StartChannel+cntChannel] = (MatrixFactor[StartChannel+cntChannel] * FactorX) + (Update_MatrixFactor[StartChannel+cntChannel] * FactorY);
		}
	}

	{
		int cntInputChannel;
		int cntOutputChannel;

		for (cntOutputChannel = 0; cntOutputChannel < NUMBEROFBUSSMATRIXOUTPUTCHANNELS; cntOutputChannel++)
		{
			float sum = 0;
			for (cntInputChannel = 0; cntInputChannel < NUMBEROFBUSSMATRIXINPUTCHANNELS; cntInputChannel++)
			{	
				sum += BussMatrixInput[cntInputChannel] * BussMatrixFactor[cntOutputChannel+cntInputChannel*NUMBEROFBUSSMATRIXOUTPUTCHANNELS];
			}
			BussMatrixOutput[cntOutputChannel] = sum*BussMatrixFactor[NUMBEROFBUSSMATRIXINPUTCHANNELS*NUMBEROFBUSSMATRIXOUTPUTCHANNELS+cntOutputChannel];
		}
	}

	{
		int cntInputChannel;
		int cntOutputChannel;

		for (cntOutputChannel = 0; cntOutputChannel < NUMBEROFMONITORMATRIXOUTPUTCHANNELS; cntOutputChannel++)
		{
			float sum = 0;
			for (cntInputChannel = 0; cntInputChannel < NUMBEROFMONITORMATRIXINPUTCHANNELS; cntInputChannel++)
			{	
				sum += MonitorMatrixInput[cntInputChannel] * MonitorMatrixFactor[cntOutputChannel+cntInputChannel*NUMBEROFMONITORMATRIXOUTPUTCHANNELS];
		}
			MonitorMatrixOutput[cntOutputChannel] = sum * MonitorMatrixFactor[NUMBEROFMONITORMATRIXINPUTCHANNELS*NUMBEROFMONITORMATRIXOUTPUTCHANNELS+cntOutputChannel];
		}
	}

	{
		const float FactorY = VUReleaseFactor;
		int cntChannel;

		for (cntChannel=0;cntChannel<NUMBEROFBUSSMATRIXOUTPUTCHANNELS; cntChannel++)
		{
			float AbsoluteAudio = _fabsf(MonitorMatrixInput[cntChannel]);				
			if (AbsoluteAudio>BussMeterPPM[cntChannel])
				BussMeterPPM[cntChannel] = AbsoluteAudio; 				
			BussMeterVU[cntChannel] = (BussMeterVU[cntChannel]*FactorY)+((1-FactorY)*AbsoluteAudio);
		}

		for (cntChannel=0;cntChannel<NUMBEROFMONITORMATRIXOUTPUTCHANNELS; cntChannel++)
		{
			float AbsoluteAudio = _fabsf(MonitorMatrixOutput[cntChannel]);				
			if (AbsoluteAudio>MonitorMeterPPM[cntChannel])
				MonitorMeterPPM[cntChannel] = AbsoluteAudio; 				
			MonitorMeterVU[cntChannel] = (MonitorMeterVU[cntChannel]*FactorY)+((1-FactorY)*AbsoluteAudio);
		}
	}

	{
		int cntChannel;
		float ReleaseFactor;
		for (cntChannel=0;cntChannel<NUMBEROFBUSSMATRIXOUTPUTCHANNELS/2; cntChannel++)
		{
			float Mono = MonitorMatrixInput[(cntChannel<<1)+0]+MonitorMatrixInput[(cntChannel<<1)+0+1];
			float Phase = 0;
			if (Mono != 0)
			{
				float Difference = MonitorMatrixInput[(cntChannel<<1)+0]-MonitorMatrixInput[(cntChannel<<1)+0+1];
				float X0 = _rcpsp(Mono);
				float X1 = X0 * (2.0 - Mono * X0);
				float X2 = X1 * (2.0 - Mono * X1);
				Phase = _fabsf(Difference * X2);
			}
			if (Phase<PhaseRMS[cntChannel])
			{	
				ReleaseFactor = 1-PhaseRelease;
			}
			else if (Phase>PhaseRMS[cntChannel])
			{
				ReleaseFactor = 1+PhaseRelease;
			}
	
			PhaseRMS[cntChannel] *= ReleaseFactor;
			if (PhaseRMS[cntChannel] < 0.01)
					PhaseRMS[cntChannel] = 0.01;
		}
		for (cntChannel=0;cntChannel<NUMBEROFMONITORMATRIXOUTPUTCHANNELS/2; cntChannel++)
		{
			float Mono = MonitorMatrixOutput[(cntChannel<<1)+0]+MonitorMatrixOutput[(cntChannel<<1)+1];
			float Phase = 0;
			if (Mono != 0)
			{
				float Difference = MonitorMatrixOutput[(cntChannel<<1)+0]-MonitorMatrixOutput[(cntChannel<<1)+1];
				float X0 = _rcpsp(Mono);
				float X1 = X0 * (2.0 - Mono * X0);
				float X2 = X1 * (2.0 - Mono * X1);
				Phase = _fabsf(Difference * X2);
			}

			if (Phase<PhaseRMS[(NUMBEROFBUSSMATRIXOUTPUTCHANNELS/2)+cntChannel])
			{
				ReleaseFactor = 1-PhaseRelease;
			}
			else if (Phase>PhaseRMS[(NUMBEROFBUSSMATRIXOUTPUTCHANNELS/2)+cntChannel])
			{
				ReleaseFactor = 1+PhaseRelease;
			}

	
			PhaseRMS[(NUMBEROFBUSSMATRIXOUTPUTCHANNELS/2)+cntChannel] *= ReleaseFactor;
			if (PhaseRMS[(NUMBEROFBUSSMATRIXOUTPUTCHANNELS/2)+cntChannel] < 0.01)
					PhaseRMS[(NUMBEROFBUSSMATRIXOUTPUTCHANNELS/2)+cntChannel] = 0.01;
		}
	}

	//Mix minus
	{
		int cntChannel;

		for (cntChannel=0;cntChannel<NUMBEROFBUSSMATRIXINPUTCHANNELS; cntChannel++)
		{
			MixMinusDelay[cntMixMinusDelayTop][cntChannel] = BussMatrixInput[cntChannel];
		}
		cntMixMinusDelayTop++;
		cntMixMinusDelayTop&=0xF;

		for (cntChannel=0;cntChannel<NUMBEROFBUSSMATRIXINPUTCHANNELS; cntChannel++)
		{
			//remove input from the selected buss for this mix-minus
			MixMinus[cntChannel] = MonitorMatrixInput[SelectedMixMinusBuss[cntChannel]];
			MixMinus[cntChannel] -= (MixMinusDelay[cntMixMinusDelayBottom][cntChannel]*BussMatrixFactor[SelectedMixMinusBuss[cntChannel]+(cntChannel*NUMBEROFBUSSMATRIXOUTPUTCHANNELS)])*BussMatrixFactor[NUMBEROFBUSSMATRIXINPUTCHANNELS*NUMBEROFBUSSMATRIXOUTPUTCHANNELS+SelectedMixMinusBuss[cntChannel]];
		}
		cntMixMinusDelayBottom++;
		cntMixMinusDelayBottom&=0xF;

		//Make them mono
		for (cntChannel=0;cntChannel<NUMBEROFBUSSMATRIXINPUTCHANNELS/2; cntChannel++)
		{
			MonoMixMinusOutput[cntChannel] = (MixMinus[(cntChannel<<1)+0]+MixMinus[(cntChannel<<1)+1])/2;
		}
	}


	{
		int cntChannel;

		for (cntChannel=0; cntChannel<32; cntChannel++)
		{
			OutputBuffer[cntChannel] = _spint(BussMatrixOutput[cntChannel]);
		}
		for (cntChannel=0; cntChannel<8; cntChannel++)
		{
			OutputBuffer[32+cntChannel] = _spint(MonitorMatrixOutput[cntChannel]);
		}
		for (cntChannel=0; cntChannel<32; cntChannel++)
		{
			OutputBuffer[48+cntChannel] = _spint(MonoMixMinusOutput[cntChannel]);
		}
/*
		for (cntChannel=0; cntChannel<16; cntChannel++)
		{
			OutputBuffer[cntChannel] = _spint(BussMatrixOutput[cntChannel]);
		}
		for (cntChannel=0; cntChannel<8; cntChannel++)
		{
			OutputBuffer[16+cntChannel] = _spint(MonitorMatrixOutput[cntChannel]);
		}
		for (cntChannel=0; cntChannel<16; cntChannel++)
		{
			OutputBuffer[32+cntChannel] = _spint(MonoMixMinusOutput[cntChannel]);
		}
*/
	}
}


