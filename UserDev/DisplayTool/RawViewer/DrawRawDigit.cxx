#ifndef LARLITE_DRAWRAWDIGIT_CXX
#define LARLITE_DRAWRAWDIGIT_CXX


#include "LArUtil/DetectorProperties.h"
#include "DrawRawDigit.h"
#include "DataFormat/rawdigit.h"

namespace evd {

  DrawRawDigit::DrawRawDigit(){ 
    _name="DrawRawDigit"; 
    producer = "daq";

    // Initialize whether or not to save the data:
    _save_data = true;

    // And whether or not to correct the data:
    _correct_data = true;

    stepSize.clear();
    stepSize.push_back(16);
    stepSize.push_back(16);
    stepSize.push_back(2*16);

  }



  bool DrawRawDigit::initialize() {

    //
    // This function is called in the beggining of event loop
    // Do all variable initialization you wish to do here.
    // If you have a histogram to fill in the event loop, for example,
    // here is a good place to create one on the heap (i.e. "new TH1D"). 
    //
    //
    

    for (unsigned int p = 0; p < geoService -> Nviews(); p ++){
      setXDimension(geoService->Nwires(p), p);
      setYDimension(detProp -> ReadOutWindowSize(), p);
    }
    initDataHolder();

    // Also initalize the space to hold the pedestal and RMS by Plane
    pedestalByPlane.resize(geoService->Nviews());
    rmsByPlane.resize(geoService->Nviews());
    rmsByPlaneCorrected.resize(geoService->Nviews());

    for (unsigned int p = 0; p < geoService -> Nviews(); p ++){
      pedestalByPlane.at(p).resize(geoService->Nwires(p));
      rmsByPlane.at(p).resize(geoService->Nwires(p));
      rmsByPlaneCorrected.at(p).resize(geoService->Nwires(p));
    }

    return true;

  }
  
  bool DrawRawDigit::analyze(larlite::storage_manager* storage) {
  
    //
    // Do your event-by-event analysis here. This function is called for 
    // each event in the loop. You have "storage" pointer which contains 
    // event-wise data. To see what is available, check the "Manual.pdf":
    //
    // http://microboone-docdb.fnal.gov:8080/cgi-bin/ShowDocument?docid=3183
    // 
    // Or you can refer to Base/DataFormatConstants.hh for available data type
    // enum values. Here is one example of getting PMT waveform collection.
    //
    // event_fifo* my_pmtfifo_v = (event_fifo*)(storage->get_data(DATA::PMFIFO));
    //
    // if( event_fifo )
    //
    //   std::cout << "Event ID: " << my_pmtfifo_v->event_id() << std::endl;
    //


    // This is an event viewer.  In particular, this handles raw wire signal drawing.
    // So, obviously, first thing to do is to get the wires.
    auto RawDigitHandle = storage->get_data<larlite::event_rawdigit>(producer);

    run = RawDigitHandle->run();
    subrun = RawDigitHandle->subrun();
    event = RawDigitHandle->event_id();

    float rmsMinBadWire = 1.0;

    badWireMapByPlane.resize(geoService->Nplanes());

    for (auto const& rawdigit: *RawDigitHandle){
      unsigned int ch = rawdigit.Channel();
      if (ch >= 8254) continue;

      unsigned int wire = geoService->ChannelToWire(ch);
      unsigned int plane = geoService->ChannelToPlane(ch);
      if (badWireMapByPlane.at(plane).size() < geoService -> Nwires(plane))
        badWireMapByPlane.at(plane).resize(geoService->Nwires(plane));

      if (wire > geoService -> Nwires(plane))
        continue;

      // There is one remaining mapping issue, perhaps:
      if (plane == 1){
        if (wire > 63 && wire < 96){
          wire += 320;
        }
        else if (wire > 383 && wire < 416){
          wire -= 320;
        }

        // Now fix the rest:
        if (wire < 31)
          wire += 64;
        else if (wire > 31 && wire < 64 ){
          int wireAnchor = wire - (wire % 32);
          wire = wireAnchor + 32 - (wire % 32);
        }
        else if (wire > 63 && wire < 96){
          wire -= 64;
        }

      }



      int offset = wire * detProp -> ReadOutWindowSize();
      // convert short ADCs to float
      
      // Get the pedestal for this channel:
      int nPedPoints = 500;
      std::vector<float> pedestal;
      pedestal.resize(nPedPoints);
      
      // Determine the distance between the pedestal points allowed:
      int pedStepSize =  detProp->ReadOutWindowSize()/ nPedPoints;

      for (int j = 0; j < nPedPoints; j++){
        pedestal.at(j) = rawdigit.ADC(j*pedStepSize);
      }
      std::sort(pedestal.begin(),pedestal.end());
      float ped = 0.5*pedestal.at(nPedPoints/2 - 1) + 0.5*pedestal.at(nPedPoints/2);

      // Set the pedestal to be stored, if needed
      pedestalByPlane.at(plane).at(wire) = ped;

      int i = 0;
      // Calculate an rms here to spot bad wires
      float rms = 0.0;
      for (auto & adc : rawdigit.ADCs()){
        // _planeData.at(plane).at(offset + i) = adc;
        _planeData.at(plane).at(offset + i) = adc - ped;
        rms += (adc-ped)*(adc-ped);
        i++;
      }

      rms /= i;

      // Store the rms for saving:
      rmsByPlane.at(plane).at(wire) = rms;

      if (rms < rmsMinBadWire)
        badWireMapByPlane.at(plane).at(wire) = true;
    }

    if (_correct_data || _save_data)
      correctData();

    return true;
  }

  bool DrawRawDigit::finalize() {

    // This function is called at the end of event loop.
    // Do all variable finalization you wish to do here.
    // If you need, you can store your ROOT class instance in the output
    // file. You have an access to the output file through "_fout" pointer.
    //
    // Say you made a histogram pointer h1 to store. You can do this:
    //
    // if(_fout) { _fout->cd(); h1->Write(); }
    //
    // else 
    //   print(MSG::ERROR,__FUNCTION__,"Did not find an output file pointer!!! File not opened?");
    //
  


    return true;
  }

  void DrawRawDigit::correctData(){

    // Loops over the data, figures out the coherent noise, and removes it.
    // If requested, it saves out the pedestal, rms, and badChannelFlag for each wire
    // If requested, it saves the subtracted wave form for each block as well as the
    //  size of the blocks in each plane (so that the blocks can be determined later)
    // If _correct_data is true, it saves out the corrected rms as well as
    //  the uncorrected rms


    // Contains the wave form to subtract [plane][block][tick]
    std::vector<std::vector<std::vector<float> > > _subtractionWaveForm;

    // Declare all the stuff necessary to save, but don't initialize unless used


    // Save one file per event ...
    TFile * _out; 
    TTree * _tree;
    


    // Determine how many steps are needed, and make space for all of the waveforms
    if (_correct_data){
      _subtractionWaveForm.resize(geoService -> Nplanes());
    
      for (unsigned int p = 0; p < geoService -> Nplanes(); p++){
        int nSteps = geoService -> Nwires(p) / stepSize.at(p);
        _subtractionWaveForm.at(p).resize(nSteps);
        for (auto & vec : _subtractionWaveForm.at(p))
          vec.resize(detProp->ReadOutWindowSize());
      }
    }

    // Initialize all of the variables and branches possible
    if (_save_data){


      // Setup the file and the ttree
      // 
      char nameFile[100];
      sprintf(nameFile,"RawDigitAna_%i_%i_%i.root",run, subrun,event);
      
      _out = new TFile(nameFile,"RECREATE");
      _out -> cd();
      _tree = new TTree("waveformsub","waveformsub");
      
      // Save the run, subrun, and event number for redundancy
      _tree -> Branch("run",&run);
      _tree -> Branch("subrun",&subrun);
      _tree -> Branch("event",&event);

      // Save the stepSize too
      _tree -> Branch("stepSize",&stepSize);

      // Save the pedestals, rms, and corrected rms by plane
      for (unsigned int p = 0; p < geoService -> Nviews(); p ++){
        _tree -> Branch(Form("pedestal_%u",p), &(pedestalByPlane.at(p)[0]));
        _tree -> Branch(Form("rms_%u",p),      &(rmsByPlane.at(p)[0]));
        if (_correct_data)
          _tree -> Branch(Form("rmsCorrected_%u",p),  &(rmsByPlaneCorrected.at(p)[0]));
      }

    }


    // Only perform the median subtraction if we're correcting the data:

    if (_correct_data){

      // We want to save the corrected RMS if we're doing the correction
      // Make sure it's zeroed out before the loops over steps

      // Do a median subtraction to remove the correlated noise
      for (unsigned int plane = 0; plane < geoService -> Nplanes(); plane ++){

        rmsByPlaneCorrected.at(plane).clear();
        rmsByPlaneCorrected.at(plane).resize(geoService->Nwires(plane));


        // Figure out how many steps to take across this wire plane
        unsigned int nSteps = geoService -> Nwires(plane) / stepSize.at(plane);
        
        for (unsigned int step = 0; step < nSteps; step ++){
          

          // Within each step, loop over the ticks.
          // For each tick, loop over each wire and find the median.
          // Then, subtract the median from each tick.
          
          for (unsigned int tick = 0; tick < detProp -> ReadOutWindowSize(); tick ++){
           
            // Get the median across this step's set of wires.
            unsigned int wireStart = step*stepSize.at(plane);
           
            // Use this vector to store the horizontal slice of values
            std::vector<float> vals;
            // Since we're skipping bad wires, don't want spurious zeros
            // so only push back values and don't intialize the whole space.  Do, however,
            // allocate it
            vals.reserve(stepSize.at(plane));

            // Loop over this block of wires and get the median value for that tick
            for (unsigned int wire = wireStart; wire < wireStart + stepSize.at(plane); wire ++){
              // skip bad wires
              if (badWireMapByPlane.at(plane)[wire]) continue;
              int offset = wire*detProp->ReadOutWindowSize();
              // if (plane != 2)
              vals.push_back(_planeData.at(plane).at(offset + tick));

              // Keep this commented out, for now, but it's pretty wrong.
              // // do a rolling average for collection plane:
              
              // else{
              //   unsigned int averageSize = 0;
              //   if (tick < averageSize || tick >= detProp->ReadOutWindowSize() - averageSize)
              //     vals.at(wire-wireStart) = _planeData.at(plane).at(offset + tick);
              //   else{
              //     for (unsigned int workingTick = tick - averageSize; workingTick <= tick + averageSize; workingTick ++ )
              //       vals.at(wire-wireStart) += _planeData.at(plane).at(offset + workingTick);
              //     vals.at(wire-wireStart) /= (2*averageSize + 1);
              //   }
              // }
            }



            float median = 0;

            if (vals.size() > stepSize.at(plane)/4){
              // Calculate the median:
              sort(vals.begin(), vals.end());

              if (vals.size() % 2 == 0){
                median =  0.5* vals.at(vals.size()/2) 
                        + 0.5* vals.at(vals.size()/2 -1);
              }
              else{
                median = vals.at((int)vals.size()/2);
              }
            }

            // Now subtract the median from the values:
            for (unsigned int wire = wireStart; wire < wireStart + stepSize.at(plane); wire ++){
              // Skip bad wires
              int offset = wire*detProp->ReadOutWindowSize();
              rmsByPlaneCorrected.at(plane).at(wire) 
                += pow((pedestalByPlane.at(plane).at(wire) - _planeData.at(plane).at(offset+tick) - median), 2);
              if (badWireMapByPlane.at(plane)[wire]) continue;
              _planeData.at(plane).at(offset + tick) -= median;
              // While we're here, do stuff to get the corrected RMS

            }


            // Save the subtraction waveform:
            _subtractionWaveForm.at(plane).at(step).at(tick) = median;

            // // Do a subtraction of the U plane wires with V plane values
            // if(plane == 0) {
            //   if ( step <= 7 || step >= 25-7) continue;
            //   int U_wire = wireStart - 672;

            //   for (unsigned int wire = U_wire; wire < U_wire + stepSize.at(plane); wire ++){
            //     int offset = wire*detProp->ReadOutWindowSize();
            //     _planeData.at(plane+1).at(offset + tick) += median;
            //   }
            // }
          } // loop over ticks

          // Now that the tick loop is finished, the wave form is finalized
          // 

          // Copy the data to the tgraph so that it can be stored, and set up the branches
          

          char name[100];
          sprintf(name,"subwaveform_%u_%u",plane,step);
          _tree -> Branch(name, &(_subtractionWaveForm.at(plane).at(step)) );
        } // loop over steps

        // Divide the RMS by the number of ticks (which is actually 9595, not 9600)
        for (auto & wire : rmsByPlaneCorrected.at(plane)){
          wire /= 9595.0;
        }

      } // loop over planes

    }


    _tree -> Fill();

    std::cout << "_out is " << _out << std::endl;

    _tree -> Write();
    _out -> Close();

    return;


  }


}
#endif



      // // TEMPORARY: fix collection mapping.
      // if (plane == 2){
      //   // std::cout << "Wire from " << wire;
      //   int wireAnchor = wire - (wire % 32);
      //   wire = wireAnchor + 32 - (wire % 32) - 1;
      //   // std::cout << " to " << wire <<std::endl;
      // }
      // if (plane == 1){
      //   if (wire < 672){
      //      int wireTemp = 0;
      //      /*// 0
      //      if (wire > -1 && wire <= 31)
      //         {wireTemp = wire + 64; }
      //      // 1
      //      if (wire > 31 && wire <=63)
      //         { wireTemp = wire - 32; }
      //      // 2   
      //      if (wire > 63 && wire  <= 95)
      //         { wireTemp = wire - 32; }
              
      //      // 3   
      //      if (wire > 95 && wire <= 127)
      //         { wireTemp = wire + 256; }
      //      // 4
      //      if (wire > 127 && wire <= 159)
      //         { wireTemp = wire + 320; } 
      //      // 5     
      //      if (wire > 159 && wire <= 191)
      //         { wireTemp = wire + 256;}
           
      //      // 6   
      //      if (wire > 191 && wire <= 223)
      //         { wireTemp = wire +320; } 
      //      // 7   
      //      if (wire > 223 && wire <= 255)
      //         { wireTemp = wire  +256; } 
      //      // 8  
      //      if (wire > 255 && wire <= 287)
      //         { wireTemp = wire +320; }
           
      //      // 9   
      //      if (wire > 287 && wire <= 319)
      //         { wireTemp = wire +256; }
      //      // 10
      //      if (wire > 319 && wire <= 351)
      //         { wireTemp = wire  +320; }
      //      // 11
      //      if (wire > 351 && wire <= 383)
      //         { wireTemp = wire +256; }
           
      //      // 12   
      //      if (wire > 383 && wire <= 415)
      //         { wireTemp = wire -256; }
           
      //      // 13
      //      if (wire > 415 && wire <= 447)
      //         { wireTemp = wire -320; }
      //      // 14
      //      if (wire > 447 && wire <= 479)
      //         { wireTemp = wire -256; }
             
      //      // 15   
      //      if (wire > 479 && wire <= 511)
      //         { wireTemp = wire  -320; }
      //      // 16                                          
      //      if (wire > 511 && wire <= 543)
      //         { wireTemp = wire -256; }
      //      // 17   
      //      if (wire > 543 && wire <= 575)
      //         { wireTemp = wire  -320; }
           
      //      // 18                 
      //      if (wire > 575 && wire <= 607)
      //         { wireTemp = wire  -256; }
      //      // 19                 
      //      if (wire > 607 && wire <= 639)
      //         { wireTemp = wire  -320; }
      //      // 20   
      //      if (wire > 639 && wire <= 671)
      //         { wireTemp = wire  -256; }                 
      //      */
           
           
      //      // 0
      //      if (wire > -1 && wire <= 31)
      //         {wireTemp = wire + 32; }
      //      // 1
      //      if (wire > 31 && wire <=63)
      //         { wireTemp = wire + 32; }
      //      // 2   
      //      if (wire > 63 && wire  <= 95)
      //         { wireTemp = wire - 64; }
              
      //      // 3   
      //      if (wire > 95 && wire <= 127)
      //         { wireTemp = wire + 320; }
      //      // 4
      //      if (wire > 127 && wire <= 159)
      //         { wireTemp = wire + 256; } 
      //      // 5     
      //      if (wire > 159 && wire <= 191)
      //         { wireTemp = wire + 320;}
           
      //      // 6   
      //      if (wire > 191 && wire <= 223)
      //         { wireTemp = wire +256; } 
      //      // 7   
      //      if (wire > 223 && wire <= 255)
      //         { wireTemp = wire  +320; } 
      //      // 8  
      //      if (wire > 255 && wire <= 287)
      //         { wireTemp = wire +256; }
           
      //      // 9   
      //      if (wire > 287 && wire <= 319)
      //         { wireTemp = wire +320; }
      //      // 10
      //      if (wire > 319 && wire <= 351)
      //         { wireTemp = wire  +256; }
      //      // 11
      //      if (wire > 351 && wire <= 383)
      //         { wireTemp = wire -256; }
           
      //      // 12   
      //      if (wire > 383 && wire <= 415)
      //         { wireTemp = wire +256; }
           
      //      // 13
      //      if (wire > 415 && wire <= 447)
      //         { wireTemp = wire -256; }
      //      // 14
      //      if (wire > 447 && wire <= 479)
      //         { wireTemp = wire -320; }
             
      //      // 15   
      //      if (wire > 479 && wire <= 511)
      //         { wireTemp = wire  -256; }
      //      // 16                                          
      //      if (wire > 511 && wire <= 543)
      //         { wireTemp = wire -320; }
      //      // 17   
      //      if (wire > 543 && wire <= 575)
      //         { wireTemp = wire  -256; }
           
      //      // 18                 
      //      if (wire > 575 && wire <= 607)
      //         { wireTemp = wire  -320; }
      //      // 19                 
      //      if (wire > 607 && wire <= 639)
      //         { wireTemp = wire  -256; }
      //      // 20   
      //      if (wire > 639 && wire <= 671)
      //         { wireTemp = wire  -320; } 
                            
      //       // int wireTemp = wire - 32;
      //      int wireAnchor = wireTemp - (wireTemp % 32);
      //      wireTemp = wireAnchor + 32 - (wire % 32) - 1;
           
      //      if(wireTemp > 63 && wireTemp <=95)
      //         {wireTemp += 320;}
      //      if(wireTemp >383 && wireTemp <=447)
      //         {wireTemp -= 320;}
           
      //       wire = wireTemp;
      //     // }
      //     // int wireAnchor = wire - (wire % 256);
      //     // wire = wireAnchor + 256 - (wire % 256) - 1 ;
      //     //int wireAnchor = wire - (wire % 64) + 16;
      //     //wire = wireAnchor + 64 - (wire % 64) - 1;
      //     //wire = wireTemp;
      //   }

      // }

      // if (plane == 0){
      //   if (wire > 1727){
      //   int wireAnchor = wire - (wire % 32);
      //   wire = wireAnchor + 32 - (wire % 32) - 1;

      //   }
      // }

      // // Catch the situation where the hanging wires are getting set:
      // if (plane == 2){
      //   if (wire > 1727 && wire < 1824){
      //     continue;
      //   }
      // }

      // // Now map the induction blocks that should be collection to collection:
      // if (plane == 1){
      //   if (wire > 1535 && wire < 1584){
      //     plane = 2;
      //     wire = 1728 + (wire - 1535)*2 - 2;
      //     // std::cout << "wire set to " << wire << std::endl;
      //   }
      // }

      // if (plane == 0){
      //   if (wire > 863 && wire < 912){
      //     plane = 2;
      //     wire = 1728 + (wire - 863)*2 - 1 ;
      //   }
      // }

