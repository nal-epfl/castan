local mg     = require "moongen"
local memory = require "memory"
local device = require "device"
local ts     = require "timestamping"
local filter = require "filter"
local hist   = require "histogram"
local stats  = require "stats"
local timer  = require "timer"
local log    = require "log"

function configure(parser)
	parser:description("Measures latency of every packet "..
                           "in the provided pcap file.")
	parser:argument("txDev", "Device to transmit from."):convert(tonumber)
	parser:argument("rxDev", "Device to receive from."):convert(tonumber)
  parser:argument("file", "File to replay."):args(1)
	parser:option("-u --upheat", "Heatup time before beginning of latency measurement"):default(2):convert(tonumber)
	parser:option("-t --timeout", "Time to run the test"):default(0):convert(tonumber)
end

function setRate(queue, packetSize, rate_mbps)
	queue:setRate(rate_mbps - (packetSize + 4) * 8 / 1000);
end

function master(args)
	txDev = device.config{port = args.txDev, rxQueues = 3, txQueues = 3}
	rxDev = device.config{port = args.rxDev, rxQueues = 3, txQueues = 3}
	device.waitForLinks()
	local file = io.open("mf-lat.txt", "w")
	file:write("#flows rate meanLat stdevLat\n")
	setRate(txDev:getTxQueue(0), args.size, args.rate);
  -- Heatup phase
  printf("heatup  - %d secs", args.upheat);
  local timerTask = mg.startTask("timerSlave", txDev:getTxQueue(1), rxDev:getRxQueue(1), args.upheat, args.file)
  mg.waitForTasks()
  printf("heatup finished");
  mg.waitForTasks()
  -- Testing phase
  local timerTask = mg.startTask("timerSlave", txDev:getTxQueue(1), rxDev:getRxQueue(1), args.timeout, args.file)
  local latency, stdev = timerTask:wait()
  printf("total: %f latency (+-%f)", latency, stdev);
  mg.waitForTasks()
  file:write(args.rate .. " " .. latency .. " " .. stdev .. "\n")
end

local function fillUdpPacket(buf, len)
	buf:getUdpPacket():fill{
		ethSrc = queue,
		ethDst = DST_MAC,
		ip4Src = SRC_IP,
		ip4Dst = DST_IP,
		udpSrc = SRC_PORT,
		udpDst = DST_PORT,
		pktLength = len
	}
end

function timerSlave(txQueue, rxQueue, duration, fname)


	local mempool = memory.createMemPool()
	local bufs = mempool:bufArray()
	local finished = timer:new(duration)
	local hist = hist:new()
  local pcapFile = pcap:newReader(fname)
  local rxBufs = mempool:bufArray(1)
	while finished:running() and mg.running() do
    local buf = pcapFile:readSingle(bufs)
    if buf == nil then
      pcapFile:reset()
    else

      ts.syncClock(txQueue.dev, rxQueue.dev)
      rxQueue.dev:clearTimestamps()
      txQueue.dev:clearTimestamps()

      --buf:offloadUdpChecksums()
      txQueue:sendSingle(buf)

      local rx = rxQueue:tryRecv(rxBufs, 10000);

      if rxQueue.dev:hasRxTimestamp() then
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
    end
	end
	mg.sleepMillis(300)
	hist:print()
	hist:save("latency-histogram.csv")
	return hist:median(), hist:standardDeviation()

	if size < 84 then
		log:warn("Packet size %d is smaller than minimum timestamp size 84. Timestamped packets will be larger than load packets.", size)
		size = 84
	end
	local finished = timer:new(duration)
	local timestamper = ts:newUdpTimestamper(txQueue, rxQueue)
	local counter = 0
	local rateLimit = timer:new(2.1/nflows)
	local baseIP = parseIPAddress(SRC_IP_BASE)
	local baseSRCP = START_PROBE_PORT
	while finished:running() and mg.running() do
		hist:update(timestamper:measureLatency(size, function(buf)
			fillUdpPacket(buf, size)
			local pkt = buf:getUdpPacket()
			-- pkt.ip4.src:set(baseIP + counter)
			pkt.ip4.src:set(baseIP)
			pkt.udp.src = (baseSRCP + counter)
			counter = incAndWrap(counter, nflows)
		end))
		rateLimit:wait()
		rateLimit:reset()
	end
	-- print the latency stats after all the other stuff
	mg.sleepMillis(300)
	hist:print()
	hist:save("latency-histogram.csv")
	return hist:median(), hist:standardDeviation()
end

