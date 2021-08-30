
let delayGraph;
let delaySeries;
let delayASeries;


function initTempMonitor() {
  delaySeries = new TimelineDataSeries();
  delaySeries.setColor('red') 
  delayASeries = new TimelineDataSeries();
  delayASeries.setColor('green') 
  
  delayGraph = new TimelineGraphView('delayGraph','delayCanvas');
  //delayGraph.updateEndDate();
}
