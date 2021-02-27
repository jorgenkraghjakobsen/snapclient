
let delayGraph;
let delaySeries;


function initTempMonitor() {
  delaySeries = new TimelineDataSeries();
  delayGraph = new TimelineGraphView('delayGraph','delayCanvas');
  delayGraph.updateEndDate();
}
