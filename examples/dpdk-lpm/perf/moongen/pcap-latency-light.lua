local mg     = require "moongen"
local memory = require "memory"
local device = require "device"
local ts     = require "timestamping"
local filter = require "filter"
local hist   = require "histogram"
local stats  = require "stats"
local timer  = require "timer"
local log    = require "log"
local pcap   = require "pcap"

function configure(parser)
	parser:description("Generates UDP traffic and measure latencies. Edit the source to modify constants like IPs.")
	parser:argument("txDev", "Device to transmit from."):convert(tonumber)
	parser:argument("rxDev", "Device to receive from."):convert(tonumber)
  parser:argument("file", "File to replay."):args(1)
	parser:option("-u --upheat", "Heatup time before beginning of latency measurement"):default(2):convert(tonumber)
	parser:option("-t --timeout", "Time to run the test"):default(0):convert(tonumber)
end

function master(args)
	txDev = device.config{port = args.txDev, rxQueues = 1, txQueues = 1}
	rxDev = device.config{port = args.rxDev, rxQueues = 1, txQueues = 1}
	device.waitForLinks()
	local file = io.open("mf-lat.txt", "w")
	file:write("#flows rate meanLat stdevLat\n")
  -- Heatup phase
  printf("heatup  - %d secs", args.upheat);
  local timerTask = mg.startTask("timerSlave", txDev:getTxQueue(0), rxDev:getRxQueue(0), args.upheat, args.file)
  mg.waitForTasks()
  printf("heatup finished");
  mg.waitForTasks()
  -- Testing phase
  local timerTask = mg.startTask("timerSlave", txDev:getTxQueue(0), rxDev:getRxQueue(0), args.timeout, args.file)
  local latency, stdev = timerTask:wait()
  printf("total: %f latency (+-%f)", latency, stdev);
  mg.waitForTasks()
  file:write(args.rate .. " " .. latency .. " " .. stdev .. "\n")
end

function timerSlave(txQueue, rxQueue, duration, fname)
  txQueue:enableTimestamps()
  rxQueue:enableTimestamps()
	local mempool = memory.createMemPool()
	local finished = timer:new(duration)
	local hist = hist:new()
  local pcapFile = pcap:newReader(fname)
  local rxBufs = mempool:bufArray(128)
	while finished:running() and mg.running() do
    local buf = pcapFile:readSingle(mempool)
    if buf == nil then
      pcapFile:reset()
    else

      ts.syncClocks(txQueue.dev, rxQueue.dev)
      rxQueue.dev:clearTimestamps()
      -- txQueue.dev:clearTimestamps()

      --buf:offloadUdpChecksums()
      buf:enableTimestamps()
      txQueue:sendSingle(buf)
      local tx = txQueue:getTimestamp(500)
	log:warn("tx tstmp: " .. tx)

      local rx = rxQueue:tryRecv(rxBufs, 10000);
      if 0 < rx then

	      if rxQueue.dev:hasRxTimestamp() then
log:warn("timestamped")
		local rxBuf = rxBufs[1]
		if rxBuf:hasTimestamp() then
		  local rxTs = rxQueue:getTimestamp(nil)
		  local lat = rxTs - tx
		  if lat < 0 or 2*10000 < lat then
		    hist:update(lat)
		  else
		    log:warn("Unrealistic latency detected")
		  end
		end
	      else
		rxBufs:freeAll();
	      end
      else

      end
    end
	end
	mg.sleepMillis(300)
	hist:print()
	hist:save("latency-histogram.csv")
	return hist:median(), hist:standardDeviation()
end

