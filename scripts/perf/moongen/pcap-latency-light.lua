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
  -- Heatup phase
  --[[
  printf("heatup  - %d secs", args.upheat);
  local timerTask = mg.startTask("timerSlave", txDev:getTxQueue(0), rxDev:getRxQueue(0), args.upheat, args.file)
  mg.waitForTasks()
  printf("heatup finished. testing is commented out");
  mg.waitForTasks()
  ]]--
  -- Testing phase
  local timerTask = mg.startTask("timerSlave", txDev:getTxQueue(0), rxDev:getRxQueue(0), args.timeout, args.file)
  timerTask:wait()
  mg.waitForTasks()
end

function myMeasureLatency(txQueue, rxQueue, buf, rxBufs) 
  maxWait = 15/1000
  --txBufs:alloc(64)
  --local buf = txBufs[1]
  buf:enableTimestamps()
  ts.syncClocks(txQueue.dev, rxQueue.dev)
  txQueue:sendSingle(buf)
  local tx = txQueue:getTimestamp(500)
  local numPkts = 0
  if tx then
    local timer = timer:new(maxWait)
    while timer:running() do
      local rx = rxQueue:tryRecv(rxBufs, 1000)
      numPkts = numPkts + rx
      local timestampedPkt = rxQueue.dev:hasRxTimestamp()
      if not timestampedPkt then
        rxBufs:freeAll()
      else
        for i = 1, rx do
          local buf = rxBufs[i]
          if buf:hasTimestamp() then
            local rxTs = rxQueue:getTimestamp(nil, nil)
            if not rxTs then
              return nil, numPkts
            end
            rxBufs:freeAll()
            local lat = rxTs - tx
            if 0 < lat and lat < 2 * maxWait * 10^9 then
              return lat, numPkts
            end
          end
        end
      end
    end
    return nil, numPkts
  else
    log:warn("Failed to timestamp packet on transmission")
    timer:new(maxWait):wait()
    return nil, numPkts
  end
end

function timerSlave(txQueue, rxQueue, duration, fname)

	local file = io.open("mf-lat.txt", "w")
  local hist = hist:new()
  local pcapFile = pcap:newReader(fname)
  local finished = timer:new(duration)
  txQueue:enableTimestamps()
  --rxQueue:enableTimestampsAllPackets() -- not supported by current ixgbe driver, and maybe even by the 82599 card
  -- Important note: the NF must convert every packet into a PTP one, to enable the RX timestamps
  rxQueue:enableTimestamps()
  local mempool = memory.createMemPool()
  --[[(function(buf)
        local pkt = buf:getPtpPacket()
        pkt.eth:fill{ethType=0x88f7}
	pkt.ptp:setVersion()
  end) --(function(buf) buf:getPtpPacket():fill() end) ]]
  --local txBufs = mempool:bufArray(1)
  local rxBufs = mempool:bufArray(128)


  while finished:running() and mg.running() do
    local buf = pcapFile:readSingle(mempool)
    if buf == nil then
      pcapFile:reset()
    else
      --txBufs:alloc(64);
      --local buf = txBufs[1]
      local lat = myMeasureLatency(txQueue, rxQueue, buf, rxBufs)
      hist:update(lat)
    end
  end
  hist:print()
  if hist.numSamples == 0 then
    log:error("Received no packets.")
  end
  print()
  return hist
end

