#ifndef ERTOOL_ERALGONUESHAREDFLASHMERGER_CXX
#define ERTOOL_ERALGONUESHAREDFLASHMERGER_CXX

#include "ERAlgoNueSharedFlashMerger.h"

namespace ertool {

  ERAlgoNueSharedFlashMerger::ERAlgoNueSharedFlashMerger(const std::string& name) : AlgoBase(name)
  {
    _debug = false;
  }

  void ERAlgoNueSharedFlashMerger::Reset()
  {}

  void ERAlgoNueSharedFlashMerger::AcceptPSet(const ::fcllite::PSet& cfg)
  {
    _alg_emp.AcceptPSet(cfg);
    return;
  }

  void ERAlgoNueSharedFlashMerger::ProcessBegin()
  {
    _alg_emp.ProcessBegin();
    _alg_emp.SetMode(true);

  }

  bool ERAlgoNueSharedFlashMerger::Reconstruct(const EventData &data, ParticleGraph& graph)
  {

    FlashID_t default_flash_id = 9999999;
    FlashID_t nue_flashid = default_flash_id;
    FlashID_t other_flashid = default_flash_id + 1;

    // Hard coded 5cm distance cut. This will come from a config file!
    double dist2_cut = 25.;
    double common_dist_cut = 10.;

    // Double checking that our electron is actually an electron
    bool JZTest = true;
    double BackDist_IPMin = 5; 

    // This is our nue candidate shower
    ::ertool::Shower electron;


    /// Loop over the reconstructed particle graph and try to find a nue
    for ( auto const &nue : graph.GetParticleArray() ) {

      if ( abs(nue.PdgCode()) != 12 ) continue;

      /// Now we have a nue
      /// Get the flash for that nue, if there is one:
      try {
        nue_flashid = data.Flash(graph.GetParticle(nue.Ancestor())).FlashID();
      }
      catch ( ERException &e ) {
        //No flash found for the nue
      }

      /// If we didn't find a flash for the nue, skip this nue
      if ( nue_flashid == default_flash_id ) continue;

      /// Now that we are interested....
      //  Save the electron for later consideration
      for ( auto const &kid : nue.Children() ) {
	auto mykid = graph.GetParticle(kid);

	/// This is the electron daughter of the nue
	if ( abs(mykid.PdgCode()) == 11 ) {	  
	  electron = data.Shower(mykid);
	}
      }// End iteration through nue childern 


      /// Now we have a nue and its associated flash.
      /// Loop through every other particle in the reco particle graph that
      /// is NOT a descendant of the nue already, and see if it has a matching flash.
      for ( auto const &part : graph.GetParticleArray() ) {
		
	if(graph.GetParticle(nue.Ancestor()).HasChild(part.ID()) == true) continue;
	
        /// If this particle is already set as the child of anything else (including the nue), skip it
        if ( part.Descendant() ) continue;

        /// Get the flash for this particle
        try {
          other_flashid = data.Flash(graph.GetParticle(part.Ancestor())).FlashID();
        }
        catch ( ERException &e ) {
          //No flash found for this particle
          other_flashid = default_flash_id + 1;
        }

        /// If this particle doesn't share a flash with the nue, skip it
        if ( other_flashid != nue_flashid ) continue;

        /// Check if particle is close to the nue, and potentially add it as a child of the nue

        /// (Two cases: the particle is a track or a shower)
        if ( part.RecoType() == kTrack ) {

	  if (_debug)
            std::cout << "Found a track in the event, square distance to neutrino vtx is " << data.Track(part).front().SqDist(nue.Vertex()) << std::endl;


	  // Define how close the shower projects back to a track
	  // With which it shares a flash
	  // This could either be due to neutrons which are emitted in 
	  // true nue CC or could be the birth vertex of a pizero...
	  double BackDisIP =  _findRel.FindClosestApproach(electron,
							   data.Track(part).front());

	  // Plot out what this distance is
	  if(_debug){
	    std::cout << "That track's start point is " 
		      << BackDisIP 
		      << " from the backwards projection of the shower" << std::endl;}

          /// If start point of the track is close to the neutrino vertex
	  //JZ: "Else If" the shower points back to the track and the IP is smaller than a few cm  
          if ( data.Track(part).front().SqDist(nue.Vertex()) < dist2_cut ) {

            if (_debug)
              std::cout << "w00t! found a track (pdg = " << part.PdgCode() << ") a squared distance "
                        << data.Track(part).front().SqDist(nue.Vertex())
                        << " from the neutrino vtx! adding as child... " << std::endl;

            /// Set parentage: this track is a child of the nue
            graph.SetParentage(nue.ID(), part.ID());

          }
	  // Play the game to see if this shower is actually a photon with a long radiation length that we missed before
	  else if(JZTest == true &&  BackDisIP < BackDist_IPMin){
	    // If there shower has no siblings it is a prime candidate 
	    // for being a pizero mis-ID, so check those 
	    if(nue.Children().size() < 2){
	      double show_to_trk = sqrt(electron.Start().SqDist(data.Track(part).front()));
	      
	      // If the shower is photon like when looking at the 
	      // flash-matched track then mark as PizeroMisID
	      if(isGammaLike(electron._dedx,show_to_trk)){
		
		if(_debug)
		  std::cout << "Ain't no nu_e! " 
			    << sqrt(data.Track(part).front().SqDist(nue.Vertex())) 
			    << std::endl;
		
                graph.GetParticle(nue.ID()).SetProcess(kPiZeroMID);		
	      }


	    }
//////////// CHECKING TO SEE IF NON-ONLY CHILDERN HAVE SPECIAL TOPLOGY 
	    if(_debug){
	      if(nue.Children().size() > 1){
		std::cout << "This could be photon " 
			  << sqrt(data.Track(part).front().SqDist(nue.Vertex())) 
			  << " but : " << std::endl; 
		for ( auto const &kid : nue.Children() ) {
		  if(abs(graph.GetParticle(kid).PdgCode()) != 11){
		    std::cout << "\t\t PDG: " << graph.GetParticle(kid).PdgCode()
			      << " Show-Kid Dist " 
			      << graph.GetParticle(kid).Vertex().SqDist(electron.Start())
			      << std::endl;
		  }
		}
	      }
}////////// END 
	  }


        } // End if potential particle to merge into interaction is a track
	
        else if ( part.RecoType() == kShower ) {
          auto shr = data.Shower(part);

          if (_debug)
            std::cout << "found a shower in the event... pdg = " << part.PdgCode() << " and ID = " << part.ID() << std::endl;
          if (_debug)
            std::cout << "and distance to neutrino vertex is " << std::sqrt(data.Shower(part).Start().SqDist(nue.Vertex())) << std::endl;

          /// Let's see if the nue has an electron as a daughter
          /// (right now i'm imagining it's a gamma that was MIDd in a NC pi0 event)
          /// and let's project the electron backwards and see if it comes from
          /// a common origin with this gamma
	  
	  double common_dist = std::sqrt( _geoalgo.commonOrigin(shr, electron, true) );
	  
	  if (_debug) {
	    std::cout << "nue has a mykid that is an electron" << std::endl;
	    std::cout << "Common origin says electron to shower bkwd dist is " << common_dist << std::endl;
	    std::cout << " (common_dist = " << common_dist << ")" << std::endl;
	  }


              /// If the backward projection of the electron and the flash-matched other shower
              /// are close together, set the nue as process=kPiZeroMID
              /// In the future, perhaps the nue should be deleted here, but right now
              /// particle deletion is not implemented in particlegraph
              if (common_dist < common_dist_cut) {
                if (_debug) {
                  std::cout << "Woah! Found a shower that nearly aligns with backward projected nue electron."
                            << std::endl;
                  std::cout << "Setting the neutrino process type to kPiZeroMID!" << std::endl;
                }

                graph.GetParticle(nue.ID()).SetProcess(kPiZeroMID);

              }

        } // End if potential particle to merge into interaction is a shower
	
      } // End inner loop over reconstructed particle graph to find additional particles to merge

    } // End outer loop over reconstructed particle graph

    return true;
  }
  
  bool ERAlgoNueSharedFlashMerger::isGammaLike(const double dedx, double radlen, bool forceRadLen)
  {
    if ( _alg_emp.LL(true, dedx, radlen) < _alg_emp.LL(false, dedx, radlen) )
      {	return true;}
    
    return false;
  }
  


  void ERAlgoNueSharedFlashMerger::ProcessEnd(TFile* fout)
  {}

}

#endif
