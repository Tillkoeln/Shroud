// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The ShroudX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeshroudnode.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "darksend.h"
#include "init.h"
//#include "governance.h"
#include "shroudnode.h"
#include "shroudnode-payments.h"
#include "shroudnodeconfig.h"
#include "shroudnode-sync.h"
#include "shroudnodeman.h"
#include "util.h"
#include "validationinterface.h"

#include <boost/lexical_cast.hpp>


CShroudnode::CShroudnode() :
        vin(),
        addr(),
        pubKeyCollateralAddress(),
        pubKeyShroudnode(),
        lastPing(),
        vchSig(),
        sigTime(GetAdjustedTime()),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(0),
        nActiveState(SHROUDNODE_ENABLED),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(PROTOCOL_VERSION),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

CShroudnode::CShroudnode(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyShroudnodeNew, int nProtocolVersionIn) :
        vin(vinNew),
        addr(addrNew),
        pubKeyCollateralAddress(pubKeyCollateralAddressNew),
        pubKeyShroudnode(pubKeyShroudnodeNew),
        lastPing(),
        vchSig(),
        sigTime(GetAdjustedTime()),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(0),
        nActiveState(SHROUDNODE_ENABLED),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(nProtocolVersionIn),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

CShroudnode::CShroudnode(const CShroudnode &other) :
        vin(other.vin),
        addr(other.addr),
        pubKeyCollateralAddress(other.pubKeyCollateralAddress),
        pubKeyShroudnode(other.pubKeyShroudnode),
        lastPing(other.lastPing),
        vchSig(other.vchSig),
        sigTime(other.sigTime),
        nLastDsq(other.nLastDsq),
        nTimeLastChecked(other.nTimeLastChecked),
        nTimeLastPaid(other.nTimeLastPaid),
        nTimeLastWatchdogVote(other.nTimeLastWatchdogVote),
        nActiveState(other.nActiveState),
        nCacheCollateralBlock(other.nCacheCollateralBlock),
        nBlockLastPaid(other.nBlockLastPaid),
        nProtocolVersion(other.nProtocolVersion),
        nPoSeBanScore(other.nPoSeBanScore),
        nPoSeBanHeight(other.nPoSeBanHeight),
        fAllowMixingTx(other.fAllowMixingTx),
        fUnitTest(other.fUnitTest) {}

CShroudnode::CShroudnode(const CShroudnodeBroadcast &mnb) :
        vin(mnb.vin),
        addr(mnb.addr),
        pubKeyCollateralAddress(mnb.pubKeyCollateralAddress),
        pubKeyShroudnode(mnb.pubKeyShroudnode),
        lastPing(mnb.lastPing),
        vchSig(mnb.vchSig),
        sigTime(mnb.sigTime),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(mnb.sigTime),
        nActiveState(mnb.nActiveState),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(mnb.nProtocolVersion),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

//CSporkManager sporkManager;
//
// When a new shroudnode broadcast is sent, update our information
//
bool CShroudnode::UpdateFromNewBroadcast(CShroudnodeBroadcast &mnb) {
    if (mnb.sigTime <= sigTime && !mnb.fRecovery) return false;

    pubKeyShroudnode = mnb.pubKeyShroudnode;
    sigTime = mnb.sigTime;
    vchSig = mnb.vchSig;
    nProtocolVersion = mnb.nProtocolVersion;
    addr = mnb.addr;
    nPoSeBanScore = 0;
    nPoSeBanHeight = 0;
    nTimeLastChecked = 0;
    int nDos = 0;
    if (mnb.lastPing == CShroudnodePing() || (mnb.lastPing != CShroudnodePing() && mnb.lastPing.CheckAndUpdate(this, true, nDos))) {
        SetLastPing(mnb.lastPing);
        mnodeman.mapSeenShroudnodePing.insert(std::make_pair(lastPing.GetHash(), lastPing));
    }
    // if it matches our Shroudnode privkey...
    if (fShroudNode && pubKeyShroudnode == activeShroudnode.pubKeyShroudnode) {
        nPoSeBanScore = -SHROUDNODE_POSE_BAN_MAX_SCORE;
        if (nProtocolVersion == PROTOCOL_VERSION) {
            // ... and PROTOCOL_VERSION, then we've been remotely activated ...
            activeShroudnode.ManageState();
        } else {
            // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
            // but also do not ban the node we get this message from
            LogPrintf("CShroudnode::UpdateFromNewBroadcast -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", nProtocolVersion, PROTOCOL_VERSION);
            return false;
        }
    }
    return true;
}

//
// Deterministically calculate a given "score" for a Shroudnode depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
arith_uint256 CShroudnode::CalculateScore(const uint256 &blockHash) {
    uint256 aux = ArithToUint256(UintToArith256(vin.prevout.hash) + vin.prevout.n);

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << blockHash;
    arith_uint256 hash2 = UintToArith256(ss.GetHash());

    CHashWriter ss2(SER_GETHASH, PROTOCOL_VERSION);
    ss2 << blockHash;
    ss2 << aux;
    arith_uint256 hash3 = UintToArith256(ss2.GetHash());

    return (hash3 > hash2 ? hash3 - hash2 : hash2 - hash3);
}

void CShroudnode::Check(bool fForce) {
    LOCK(cs);

    if (ShutdownRequested()) return;

    if (!fForce && (GetTime() - nTimeLastChecked < SHROUDNODE_CHECK_SECONDS)) return;
    nTimeLastChecked = GetTime();

    LogPrint("shroudnode", "CShroudnode::Check -- Shroudnode %s is in %s state\n", vin.prevout.ToStringShort(), GetStateString());

    //once spent, stop doing the checks
    if (IsOutpointSpent()) return;

    int nHeight = 0;
    if (!fUnitTest) {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) return;

        CCoins coins;
        if (!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
            (unsigned int) vin.prevout.n >= coins.vout.size() ||
            coins.vout[vin.prevout.n].IsNull()) {
            SetStatus(SHROUDNODE_OUTPOINT_SPENT);
            LogPrint("shroudnode", "CShroudnode::Check -- Failed to find Shroudnode UTXO, shroudnode=%s\n", vin.prevout.ToStringShort());
            return;
        }

        nHeight = chainActive.Height();
    }

    if (IsPoSeBanned()) {
        if (nHeight < nPoSeBanHeight) return; // too early?
        // Otherwise give it a chance to proceed further to do all the usual checks and to change its state.
        // Shroudnode still will be on the edge and can be banned back easily if it keeps ignoring mnverify
        // or connect attempts. Will require few mnverify messages to strengthen its position in mn list.
        LogPrintf("CShroudnode::Check -- Shroudnode %s is unbanned and back in list now\n", vin.prevout.ToStringShort());
        DecreasePoSeBanScore();
    } else if (nPoSeBanScore >= SHROUDNODE_POSE_BAN_MAX_SCORE) {
        SetStatus(SHROUDNODE_POSE_BAN);
        // ban for the whole payment cycle
        nPoSeBanHeight = nHeight + mnodeman.size();
        LogPrintf("CShroudnode::Check -- Shroudnode %s is banned till block %d now\n", vin.prevout.ToStringShort(), nPoSeBanHeight);
        return;
    }

    int nActiveStatePrev = nActiveState;
    bool fOurShroudnode = fShroudNode && activeShroudnode.pubKeyShroudnode == pubKeyShroudnode;

    // shroudnode doesn't meet payment protocol requirements ...
/*    bool fRequireUpdate = nProtocolVersion < mnpayments.GetMinShroudnodePaymentsProto() ||
                          // or it's our own node and we just updated it to the new protocol but we are still waiting for activation ...
                          (fOurShroudnode && nProtocolVersion < PROTOCOL_VERSION); */

    // shroudnode doesn't meet payment protocol requirements ...
    bool fRequireUpdate = nProtocolVersion < mnpayments.GetMinShroudnodePaymentsProto() ||
                          // or it's our own node and we just updated it to the new protocol but we are still waiting for activation ...
                          (fOurShroudnode && (nProtocolVersion < MIN_SHROUDNODE_PAYMENT_PROTO_VERSION_1 || nProtocolVersion > MIN_SHROUDNODE_PAYMENT_PROTO_VERSION_2));

    if (fRequireUpdate) {
        SetStatus(SHROUDNODE_UPDATE_REQUIRED);
        if (nActiveStatePrev != nActiveState) {
            LogPrint("shroudnode", "CShroudnode::Check -- Shroudnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

    // keep old shroudnodes on start, give them a chance to receive updates...
    bool fWaitForPing = !shroudnodeSync.IsShroudnodeListSynced() && !IsPingedWithin(SHROUDNODE_MIN_MNP_SECONDS);

    if (fWaitForPing && !fOurShroudnode) {
        // ...but if it was already expired before the initial check - return right away
        if (IsExpired() || IsWatchdogExpired() || IsNewStartRequired()) {
            LogPrint("shroudnode", "CShroudnode::Check -- Shroudnode %s is in %s state, waiting for ping\n", vin.prevout.ToStringShort(), GetStateString());
            return;
        }
    }

    // don't expire if we are still in "waiting for ping" mode unless it's our own shroudnode
    if (!fWaitForPing || fOurShroudnode) {

        if (!IsPingedWithin(SHROUDNODE_NEW_START_REQUIRED_SECONDS)) {
            SetStatus(SHROUDNODE_NEW_START_REQUIRED);
            if (nActiveStatePrev != nActiveState) {
                LogPrint("shroudnode", "CShroudnode::Check -- Shroudnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        bool fWatchdogActive = shroudnodeSync.IsSynced() && mnodeman.IsWatchdogActive();
        bool fWatchdogExpired = (fWatchdogActive && ((GetTime() - nTimeLastWatchdogVote) > SHROUDNODE_WATCHDOG_MAX_SECONDS));

//        LogPrint("shroudnode", "CShroudnode::Check -- outpoint=%s, nTimeLastWatchdogVote=%d, GetTime()=%d, fWatchdogExpired=%d\n",
//                vin.prevout.ToStringShort(), nTimeLastWatchdogVote, GetTime(), fWatchdogExpired);

        if (fWatchdogExpired) {
            SetStatus(SHROUDNODE_WATCHDOG_EXPIRED);
            if (nActiveStatePrev != nActiveState) {
                LogPrint("shroudnode", "CShroudnode::Check -- Shroudnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        if (!IsPingedWithin(SHROUDNODE_EXPIRATION_SECONDS)) {
            SetStatus(SHROUDNODE_EXPIRED);
            if (nActiveStatePrev != nActiveState) {
                LogPrint("shroudnode", "CShroudnode::Check -- Shroudnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }
    }

    if (lastPing.sigTime - sigTime < SHROUDNODE_MIN_MNP_SECONDS) {
        SetStatus(SHROUDNODE_PRE_ENABLED);
        if (nActiveStatePrev != nActiveState) {
            LogPrint("shroudnode", "CShroudnode::Check -- Shroudnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

    SetStatus(SHROUDNODE_ENABLED); // OK
    if (nActiveStatePrev != nActiveState) {
        LogPrint("shroudnode", "CShroudnode::Check -- Shroudnode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
    }
}

bool CShroudnode::IsValidNetAddr() {
    return IsValidNetAddr(addr);
}

bool CShroudnode::IsValidForPayment() {
    if (nActiveState == SHROUDNODE_ENABLED) {
        return true;
    }
//    if(!sporkManager.IsSporkActive(SPORK_14_REQUIRE_SENTINEL_FLAG) &&
//       (nActiveState == SHROUDNODE_WATCHDOG_EXPIRED)) {
//        return true;
//    }

    return false;
}

bool CShroudnode::IsValidNetAddr(CService addrIn) {
    // TODO: regtest is fine with any addresses for now,
    // should probably be a bit smarter if one day we start to implement tests for this
    return Params().NetworkIDString() == CBaseChainParams::REGTEST ||
           (addrIn.IsIPv4() && IsReachable(addrIn) && addrIn.IsRoutable());
}

bool CShroudnode::IsMyShroudnode(){
    BOOST_FOREACH(CShroudnodeConfig::CShroudnodeEntry mne, shroudnodeConfig.getEntries()) {
        const std::string& txHash = mne.getTxHash();
        const std::string& outputIndex = mne.getOutputIndex();

        if(txHash==vin.prevout.hash.ToString().substr(0,64) &&
           outputIndex==to_string(vin.prevout.n))
            return true;
    }
    return false;
}

shroudnode_info_t CShroudnode::GetInfo() {
    shroudnode_info_t info;
    info.vin = vin;
    info.addr = addr;
    info.pubKeyCollateralAddress = pubKeyCollateralAddress;
    info.pubKeyShroudnode = pubKeyShroudnode;
    info.sigTime = sigTime;
    info.nLastDsq = nLastDsq;
    info.nTimeLastChecked = nTimeLastChecked;
    info.nTimeLastPaid = nTimeLastPaid;
    info.nTimeLastWatchdogVote = nTimeLastWatchdogVote;
    info.nTimeLastPing = lastPing.sigTime;
    info.nActiveState = nActiveState;
    info.nProtocolVersion = nProtocolVersion;
    info.fInfoValid = true;
    return info;
}

std::string CShroudnode::StateToString(int nStateIn) {
    switch (nStateIn) {
        case SHROUDNODE_PRE_ENABLED:
            return "PRE_ENABLED";
        case SHROUDNODE_ENABLED:
            return "ENABLED";
        case SHROUDNODE_EXPIRED:
            return "EXPIRED";
        case SHROUDNODE_OUTPOINT_SPENT:
            return "OUTPOINT_SPENT";
        case SHROUDNODE_UPDATE_REQUIRED:
            return "UPDATE_REQUIRED";
        case SHROUDNODE_WATCHDOG_EXPIRED:
            return "WATCHDOG_EXPIRED";
        case SHROUDNODE_NEW_START_REQUIRED:
            return "NEW_START_REQUIRED";
        case SHROUDNODE_POSE_BAN:
            return "POSE_BAN";
        default:
            return "UNKNOWN";
    }
}

std::string CShroudnode::GetStateString() const {
    return StateToString(nActiveState);
}

std::string CShroudnode::GetStatus() const {
    // TODO: return smth a bit more human readable here
    return GetStateString();
}

void CShroudnode::SetStatus(int newState) {
    if(nActiveState!=newState){
        nActiveState = newState;
        if(IsMyShroudnode())
            GetMainSignals().UpdatedShroudnode(*this);
    }
}

void CShroudnode::SetLastPing(CShroudnodePing newShroudnodePing) {
    if(lastPing!=newShroudnodePing){
        lastPing = newShroudnodePing;
        if(IsMyShroudnode())
            GetMainSignals().UpdatedShroudnode(*this);
    }
}

void CShroudnode::SetTimeLastPaid(int64_t newTimeLastPaid) {
     if(nTimeLastPaid!=newTimeLastPaid){
        nTimeLastPaid = newTimeLastPaid;
        if(IsMyShroudnode())
            GetMainSignals().UpdatedShroudnode(*this);
    }   
}

void CShroudnode::SetBlockLastPaid(int newBlockLastPaid) {
     if(nBlockLastPaid!=newBlockLastPaid){
        nBlockLastPaid = newBlockLastPaid;
        if(IsMyShroudnode())
            GetMainSignals().UpdatedShroudnode(*this);
    }   
}

void CShroudnode::SetRank(int newRank) {
     if(nRank!=newRank){
        nRank = newRank;
        if(nRank < 0 || nRank > mnodeman.size()) nRank = 0;
        if(IsMyShroudnode())
            GetMainSignals().UpdatedShroudnode(*this);
    }   
}

std::string CShroudnode::ToString() const {
    std::string str;
    str += "shroudnode{";
    str += addr.ToString();
    str += " ";
    str += std::to_string(nProtocolVersion);
    str += " ";
    str += vin.prevout.ToStringShort();
    str += " ";
    str += CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString();
    str += " ";
    str += std::to_string(lastPing == CShroudnodePing() ? sigTime : lastPing.sigTime);
    str += " ";
    str += std::to_string(lastPing == CShroudnodePing() ? 0 : lastPing.sigTime - sigTime);
    str += " ";
    str += std::to_string(nBlockLastPaid);
    str += "}\n";
    return str;
}

UniValue CShroudnode::ToJSON() const {
    UniValue ret(UniValue::VOBJ);
    std::string payee = CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString();
    COutPoint outpoint = vin.prevout;
    UniValue outpointObj(UniValue::VOBJ);
    UniValue authorityObj(UniValue::VOBJ);
    outpointObj.push_back(Pair("txid", outpoint.hash.ToString().substr(0,64)));
    outpointObj.push_back(Pair("index", to_string(outpoint.n)));

    std::string authority = addr.ToString();
    std::string ip   = authority.substr(0, authority.find(":"));
    std::string port = authority.substr(authority.find(":")+1, authority.length());
    authorityObj.push_back(Pair("ip", ip));
    authorityObj.push_back(Pair("port", port));
    
    // get myShroudnode data
    bool isMine = false;
    string label;
    int fIndex=0;
    BOOST_FOREACH(CShroudnodeConfig::CShroudnodeEntry mne, shroudnodeConfig.getEntries()) {
        CTxIn myVin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
        if(outpoint.ToStringShort()==myVin.prevout.ToStringShort()){
            isMine = true;
            label = mne.getAlias();
            break;
        }
        fIndex++;
    }

    ret.push_back(Pair("rank", nRank));
    ret.push_back(Pair("outpoint", outpointObj));
    ret.push_back(Pair("status", GetStatus()));
    ret.push_back(Pair("protocolVersion", nProtocolVersion));
    ret.push_back(Pair("payeeAddress", payee));
    ret.push_back(Pair("lastSeen", (int64_t) lastPing.sigTime * 1000));
    ret.push_back(Pair("activeSince", (int64_t)(sigTime * 1000)));
    ret.push_back(Pair("lastPaidTime", (int64_t) GetLastPaidTime() * 1000));
    ret.push_back(Pair("lastPaidBlock", GetLastPaidBlock()));
    ret.push_back(Pair("authority", authorityObj));
    ret.push_back(Pair("isMine", isMine));
    if(isMine){
        ret.push_back(Pair("label", label));
        ret.push_back(Pair("position", fIndex));
    }

    UniValue qualify(UniValue::VOBJ);

    CShroudnode* shroudnode = const_cast <CShroudnode*> (this);
    qualify = mnodeman.GetNotQualifyReasonToUniValue(*shroudnode, chainActive.Tip()->nHeight, true, mnodeman.CountEnabled());
    ret.push_back(Pair("qualify", qualify));

    return ret;
}

int CShroudnode::GetCollateralAge() {
    int nHeight;
    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain || !chainActive.Tip()) return -1;
        nHeight = chainActive.Height();
    }

    if (nCacheCollateralBlock == 0) {
        int nInputAge = GetInputAge(vin);
        if (nInputAge > 0) {
            nCacheCollateralBlock = nHeight - nInputAge;
        } else {
            return nInputAge;
        }
    }

    return nHeight - nCacheCollateralBlock;
}

void CShroudnode::UpdateLastPaid(const CBlockIndex *pindex, int nMaxBlocksToScanBack) {
    if (!pindex) {
        LogPrintf("CShroudnode::UpdateLastPaid pindex is NULL\n");
        return;
    }

    const Consensus::Params &params = Params().GetConsensus();
    const CBlockIndex *BlockReading = pindex;

    CScript mnpayee = GetScriptForDestination(pubKeyCollateralAddress.GetID());
    LogPrint("shroudnode", "CShroudnode::UpdateLastPaidBlock -- searching for block with payment to %s\n", vin.prevout.ToStringShort());

    LOCK(cs_mapShroudnodeBlocks);

    for (int i = 0; BlockReading && BlockReading->nHeight > nBlockLastPaid && i < nMaxBlocksToScanBack; i++) {
//        LogPrintf("mnpayments.mapShroudnodeBlocks.count(BlockReading->nHeight)=%s\n", mnpayments.mapShroudnodeBlocks.count(BlockReading->nHeight));
//        LogPrintf("mnpayments.mapShroudnodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2)=%s\n", mnpayments.mapShroudnodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2));
        if (mnpayments.mapShroudnodeBlocks.count(BlockReading->nHeight) &&
            mnpayments.mapShroudnodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2)) {
            // LogPrintf("i=%s, BlockReading->nHeight=%s\n", i, BlockReading->nHeight);
            CBlock block;
            if (!ReadBlockFromDisk(block, BlockReading, Params().GetConsensus())) // shouldn't really happen
            {
                LogPrintf("ReadBlockFromDisk failed\n");
                continue;
            }
            CAmount nShroudnodePayment = GetShroudnodePayment(params, false,BlockReading->nHeight);

            BOOST_FOREACH(CTxOut txout, block.vtx[0].vout)
            if (mnpayee == txout.scriptPubKey && nShroudnodePayment == txout.nValue) {
                SetBlockLastPaid(BlockReading->nHeight);
                SetTimeLastPaid(BlockReading->nTime);
                LogPrint("shroudnode", "CShroudnode::UpdateLastPaidBlock -- searching for block with payment to %s -- found new %d\n", vin.prevout.ToStringShort(), nBlockLastPaid);
                return;
            }
        }

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    // Last payment for this shroudnode wasn't found in latest mnpayments blocks
    // or it was found in mnpayments blocks but wasn't found in the blockchain.
    // LogPrint("shroudnode", "CShroudnode::UpdateLastPaidBlock -- searching for block with payment to %s -- keeping old %d\n", vin.prevout.ToStringShort(), nBlockLastPaid);
}

bool CShroudnodeBroadcast::Create(std::string strService, std::string strKeyShroudnode, std::string strTxHash, std::string strOutputIndex, std::string &strErrorRet, CShroudnodeBroadcast &mnbRet, bool fOffline) {
    LogPrintf("CShroudnodeBroadcast::Create\n");
    CTxIn txin;
    CPubKey pubKeyCollateralAddressNew;
    CKey keyCollateralAddressNew;
    CPubKey pubKeyShroudnodeNew;
    CKey keyShroudnodeNew;
    //need correct blocks to send ping
    if (!fOffline && !shroudnodeSync.IsBlockchainSynced()) {
        strErrorRet = "Sync in progress. Must wait until sync is complete to start Shroudnode";
        LogPrintf("CShroudnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    //TODO
    if (!darkSendSigner.GetKeysFromSecret(strKeyShroudnode, keyShroudnodeNew, pubKeyShroudnodeNew)) {
        strErrorRet = strprintf("Invalid shroudnode key %s", strKeyShroudnode);
        LogPrintf("CShroudnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    if (!pwalletMain->GetShroudnodeVinAndKeys(txin, pubKeyCollateralAddressNew, keyCollateralAddressNew, strTxHash, strOutputIndex)) {
        strErrorRet = strprintf("Could not allocate txin %s:%s for shroudnode %s", strTxHash, strOutputIndex, strService);
        LogPrintf("CShroudnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    CService service = CService(strService);
    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (service.GetPort() != mainnetDefaultPort) {
            strErrorRet = strprintf("Invalid port %u for shroudnode %s, only %d is supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
            LogPrintf("CShroudnodeBroadcast::Create -- %s\n", strErrorRet);
            return false;
        }
    } else if (service.GetPort() == mainnetDefaultPort) {
        strErrorRet = strprintf("Invalid port %u for shroudnode %s, %d is the only supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
        LogPrintf("CShroudnodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    return Create(txin, CService(strService), keyCollateralAddressNew, pubKeyCollateralAddressNew, keyShroudnodeNew, pubKeyShroudnodeNew, strErrorRet, mnbRet);
}

bool CShroudnodeBroadcast::Create(CTxIn txin, CService service, CKey keyCollateralAddressNew, CPubKey pubKeyCollateralAddressNew, CKey keyShroudnodeNew, CPubKey pubKeyShroudnodeNew, std::string &strErrorRet, CShroudnodeBroadcast &mnbRet) {
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    LogPrint("shroudnode", "CShroudnodeBroadcast::Create -- pubKeyCollateralAddressNew = %s, pubKeyShroudnodeNew.GetID() = %s\n",
             CBitcoinAddress(pubKeyCollateralAddressNew.GetID()).ToString(),
             pubKeyShroudnodeNew.GetID().ToString());


    CShroudnodePing mnp(txin);
    if (!mnp.Sign(keyShroudnodeNew, pubKeyShroudnodeNew)) {
        strErrorRet = strprintf("Failed to sign ping, shroudnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CShroudnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CShroudnodeBroadcast();
        return false;
    }

    int nHeight = chainActive.Height();
    if (nHeight < ZC_MODULUS_V2_START_BLOCK) {
        mnbRet = CShroudnodeBroadcast(service, txin, pubKeyCollateralAddressNew, pubKeyShroudnodeNew, MIN_PEER_PROTO_VERSION);
    } else {
        mnbRet = CShroudnodeBroadcast(service, txin, pubKeyCollateralAddressNew, pubKeyShroudnodeNew, PROTOCOL_VERSION);
    }

    if (!mnbRet.IsValidNetAddr()) {
        strErrorRet = strprintf("Invalid IP address, shroudnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CShroudnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CShroudnodeBroadcast();
        return false;
    }
    mnbRet.SetLastPing(mnp);
    if (!mnbRet.Sign(keyCollateralAddressNew)) {
        strErrorRet = strprintf("Failed to sign broadcast, shroudnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CShroudnodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CShroudnodeBroadcast();
        return false;
    }

    return true;
}

bool CShroudnodeBroadcast::SimpleCheck(int &nDos) {
    nDos = 0;

    // make sure addr is valid
    if (!IsValidNetAddr()) {
        LogPrintf("CShroudnodeBroadcast::SimpleCheck -- Invalid addr, rejected: shroudnode=%s  addr=%s\n",
                  vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    // make sure signature isn't in the future (past is OK)
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CShroudnodeBroadcast::SimpleCheck -- Signature rejected, too far into the future: shroudnode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    // empty ping or incorrect sigTime/unknown blockhash
    if (lastPing == CShroudnodePing() || !lastPing.SimpleCheck(nDos)) {
        // one of us is probably forked or smth, just mark it as expired and check the rest of the rules
        SetStatus(SHROUDNODE_EXPIRED);
    }

    if (nProtocolVersion < mnpayments.GetMinShroudnodePaymentsProto()) {
        LogPrintf("CShroudnodeBroadcast::SimpleCheck -- ignoring outdated Shroudnode: shroudnode=%s  nProtocolVersion=%d\n", vin.prevout.ToStringShort(), nProtocolVersion);
        return false;
    }

    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    if (pubkeyScript.size() != 25) {
        LogPrintf("CShroudnodeBroadcast::SimpleCheck -- pubKeyCollateralAddress has the wrong size\n");
        nDos = 100;
        return false;
    }

    CScript pubkeyScript2;
    pubkeyScript2 = GetScriptForDestination(pubKeyShroudnode.GetID());

    if (pubkeyScript2.size() != 25) {
        LogPrintf("CShroudnodeBroadcast::SimpleCheck -- pubKeyShroudnode has the wrong size\n");
        nDos = 100;
        return false;
    }

    if (!vin.scriptSig.empty()) {
        LogPrintf("CShroudnodeBroadcast::SimpleCheck -- Ignore Not Empty ScriptSig %s\n", vin.ToString());
        nDos = 100;
        return false;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (addr.GetPort() != mainnetDefaultPort) return false;
    } else if (addr.GetPort() == mainnetDefaultPort) return false;

    return true;
}

bool CShroudnodeBroadcast::Update(CShroudnode *pmn, int &nDos) {
    nDos = 0;

    if (pmn->sigTime == sigTime && !fRecovery) {
        // mapSeenShroudnodeBroadcast in CShroudnodeMan::CheckMnbAndUpdateShroudnodeList should filter legit duplicates
        // but this still can happen if we just started, which is ok, just do nothing here.
        return false;
    }

    // this broadcast is older than the one that we already have - it's bad and should never happen
    // unless someone is doing something fishy
    if (pmn->sigTime > sigTime) {
        LogPrintf("CShroudnodeBroadcast::Update -- Bad sigTime %d (existing broadcast is at %d) for Shroudnode %s %s\n",
                  sigTime, pmn->sigTime, vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    pmn->Check();

    // shroudnode is banned by PoSe
    if (pmn->IsPoSeBanned()) {
        LogPrintf("CShroudnodeBroadcast::Update -- Banned by PoSe, shroudnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // IsVnAssociatedWithPubkey is validated once in CheckOutpoint, after that they just need to match
    if (pmn->pubKeyCollateralAddress != pubKeyCollateralAddress) {
        LogPrintf("CShroudnodeBroadcast::Update -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrintf("CShroudnodeBroadcast::Update -- CheckSignature() failed, shroudnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // if ther was no shroudnode broadcast recently or if it matches our Shroudnode privkey...
    if (!pmn->IsBroadcastedWithin(SHROUDNODE_MIN_MNB_SECONDS) || (fShroudNode && pubKeyShroudnode == activeShroudnode.pubKeyShroudnode)) {
        // take the newest entry
        LogPrintf("CShroudnodeBroadcast::Update -- Got UPDATED Shroudnode entry: addr=%s\n", addr.ToString());
        if (pmn->UpdateFromNewBroadcast((*this))) {
            pmn->Check();
            RelayShroudNode();
        }
        shroudnodeSync.AddedShroudnodeList();
        GetMainSignals().UpdatedShroudnode(*pmn);
    }

    return true;
}

bool CShroudnodeBroadcast::CheckOutpoint(int &nDos) {
    // we are a shroudnode with the same vin (i.e. already activated) and this mnb is ours (matches our Shroudnode privkey)
    // so nothing to do here for us
    if (fShroudNode && vin.prevout == activeShroudnode.vin.prevout && pubKeyShroudnode == activeShroudnode.pubKeyShroudnode) {
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrintf("CShroudnodeBroadcast::CheckOutpoint -- CheckSignature() failed, shroudnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            // not mnb fault, let it to be checked again later
            LogPrint("shroudnode", "CShroudnodeBroadcast::CheckOutpoint -- Failed to aquire lock, addr=%s", addr.ToString());
            mnodeman.mapSeenShroudnodeBroadcast.erase(GetHash());
            return false;
        }

        CCoins coins;
        if (!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
            (unsigned int) vin.prevout.n >= coins.vout.size() ||
            coins.vout[vin.prevout.n].IsNull()) {
            LogPrint("shroudnode", "CShroudnodeBroadcast::CheckOutpoint -- Failed to find Shroudnode UTXO, shroudnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }

        if (coins.vout[vin.prevout.n].nValue != SHROUDNODE_COIN_REQUIRED(chainActive.Height()) * COIN) {
            LogPrint("shroudnode", "CShroudnodeBroadcast::CheckOutpoint -- Shroudnode UTXO should have 50000 SHROUD, shroudnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }

        if (chainActive.Height() - coins.nHeight + 1 < Params().GetConsensus().nShroudnodeMinimumConfirmations) {
            LogPrintf("CShroudnodeBroadcast::CheckOutpoint -- Shroudnode UTXO must have at least %d confirmations, shroudnode=%s\n",
                      Params().GetConsensus().nShroudnodeMinimumConfirmations, vin.prevout.ToStringShort());
            // maybe we miss few blocks, let this mnb to be checked again later
            mnodeman.mapSeenShroudnodeBroadcast.erase(GetHash());
            return false;
        }
    }

    LogPrint("shroudnode", "CShroudnodeBroadcast::CheckOutpoint -- Shroudnode UTXO verified\n");

    // make sure the vout that was signed is related to the transaction that spawned the Shroudnode
    //  - this is expensive, so it's only done once per Shroudnode
    if (!darkSendSigner.IsVinAssociatedWithPubkey(vin, pubKeyCollateralAddress)) {
        LogPrintf("CShroudnodeMan::CheckOutpoint -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    // verify that sig time is legit in past
    // should be at least not earlier than block when 1000 IDX tx got nShroudnodeMinimumConfirmations
    uint256 hashBlock = uint256();
    CTransaction tx2;
    GetTransaction(vin.prevout.hash, tx2, Params().GetConsensus(), hashBlock, true);
    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex *pMNIndex = (*mi).second; // block for 1000 IDX tx -> 1 confirmation
            CBlockIndex *pConfIndex = chainActive[pMNIndex->nHeight + Params().GetConsensus().nShroudnodeMinimumConfirmations - 1]; // block where tx got nShroudnodeMinimumConfirmations
            if (pConfIndex->GetBlockTime() > sigTime) {
                LogPrintf("CShroudnodeBroadcast::CheckOutpoint -- Bad sigTime %d (%d conf block is at %d) for Shroudnode %s %s\n",
                          sigTime, Params().GetConsensus().nShroudnodeMinimumConfirmations, pConfIndex->GetBlockTime(), vin.prevout.ToStringShort(), addr.ToString());
                return false;
            }
        }
    }

    return true;
}

bool CShroudnodeBroadcast::Sign(CKey &keyCollateralAddress) {
    std::string strError;
    std::string strMessage;

    sigTime = GetAdjustedTime();

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                 pubKeyCollateralAddress.GetID().ToString() + pubKeyShroudnode.GetID().ToString() +
                 boost::lexical_cast<std::string>(nProtocolVersion);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, keyCollateralAddress)) {
        LogPrintf("CShroudnodeBroadcast::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
        LogPrintf("CShroudnodeBroadcast::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CShroudnodeBroadcast::CheckSignature(int &nDos) {
    std::string strMessage;
    std::string strError = "";
    nDos = 0;

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                 pubKeyCollateralAddress.GetID().ToString() + pubKeyShroudnode.GetID().ToString() +
                 boost::lexical_cast<std::string>(nProtocolVersion);

    LogPrint("shroudnode", "CShroudnodeBroadcast::CheckSignature -- strMessage: %s  pubKeyCollateralAddress address: %s  sig: %s\n", strMessage, CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString(), EncodeBase64(&vchSig[0], vchSig.size()));

    if (!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
        LogPrintf("CShroudnodeBroadcast::CheckSignature -- Got bad Shroudnode announce signature, error: %s\n", strError);
        nDos = 100;
        return false;
    }

    return true;
}

void CShroudnodeBroadcast::RelayShroudNode() {
    LogPrintf("CShroudnodeBroadcast::RelayShroudNode\n");
    CInv inv(MSG_SHROUDNODE_ANNOUNCE, GetHash());
    RelayInv(inv);
}

CShroudnodePing::CShroudnodePing(CTxIn &vinNew) {
    LOCK(cs_main);
    if (!chainActive.Tip() || chainActive.Height() < 12) return;

    vin = vinNew;
    blockHash = chainActive[chainActive.Height() - 12]->GetBlockHash();
    sigTime = GetAdjustedTime();
    vchSig = std::vector < unsigned char > ();
}

bool CShroudnodePing::Sign(CKey &keyShroudnode, CPubKey &pubKeyShroudnode) {
    std::string strError;
    std::string strShroudNodeSignMessage;

    sigTime = GetAdjustedTime();
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, keyShroudnode)) {
        LogPrintf("CShroudnodePing::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubKeyShroudnode, vchSig, strMessage, strError)) {
        LogPrintf("CShroudnodePing::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CShroudnodePing::CheckSignature(CPubKey &pubKeyShroudnode, int &nDos) {
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);
    std::string strError = "";
    nDos = 0;

    if (!darkSendSigner.VerifyMessage(pubKeyShroudnode, vchSig, strMessage, strError)) {
        LogPrintf("CShroudnodePing::CheckSignature -- Got bad Shroudnode ping signature, shroudnode=%s, error: %s\n", vin.prevout.ToStringShort(), strError);
        nDos = 33;
        return false;
    }
    return true;
}

bool CShroudnodePing::SimpleCheck(int &nDos) {
    // don't ban by default
    nDos = 0;

    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CShroudnodePing::SimpleCheck -- Signature rejected, too far into the future, shroudnode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    {
//        LOCK(cs_main);
        AssertLockHeld(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if (mi == mapBlockIndex.end()) {
            LogPrint("shroudnode", "CShroudnodePing::SimpleCheck -- Shroudnode ping is invalid, unknown block hash: shroudnode=%s blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // maybe we stuck or forked so we shouldn't ban this node, just fail to accept this ping
            // TODO: or should we also request this block?
            return false;
        }
    }
    LogPrint("shroudnode", "CShroudnodePing::SimpleCheck -- Shroudnode ping verified: shroudnode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);
    return true;
}

bool CShroudnodePing::CheckAndUpdate(CShroudnode *pmn, bool fFromNewBroadcast, int &nDos) {
    // don't ban by default
    nDos = 0;

    if (!SimpleCheck(nDos)) {
        return false;
    }

    if (pmn == NULL) {
        LogPrint("shroudnode", "CShroudnodePing::CheckAndUpdate -- Couldn't find Shroudnode entry, shroudnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    if (!fFromNewBroadcast) {
        if (pmn->IsUpdateRequired()) {
            LogPrint("shroudnode", "CShroudnodePing::CheckAndUpdate -- shroudnode protocol is outdated, shroudnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }

        if (pmn->IsNewStartRequired()) {
            LogPrint("shroudnode", "CShroudnodePing::CheckAndUpdate -- shroudnode is completely expired, new start is required, shroudnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
    }

    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if ((*mi).second && (*mi).second->nHeight < chainActive.Height() - 24) {
            // LogPrintf("CShroudnodePing::CheckAndUpdate -- Shroudnode ping is invalid, block hash is too old: shroudnode=%s  blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // nDos = 1;
            return false;
        }
    }

    LogPrint("shroudnode", "CShroudnodePing::CheckAndUpdate -- New ping: shroudnode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);

    // LogPrintf("mnping - Found corresponding mn for vin: %s\n", vin.prevout.ToStringShort());
    // update only if there is no known ping for this shroudnode or
    // last ping was more then SHROUDNODE_MIN_MNP_SECONDS-60 ago comparing to this one
    if (pmn->IsPingedWithin(SHROUDNODE_MIN_MNP_SECONDS - 60, sigTime)) {
        LogPrint("shroudnode", "CShroudnodePing::CheckAndUpdate -- Shroudnode ping arrived too early, shroudnode=%s\n", vin.prevout.ToStringShort());
        //nDos = 1; //disable, this is happening frequently and causing banned peers
        return false;
    }

    if (!CheckSignature(pmn->pubKeyShroudnode, nDos)) return false;

    // so, ping seems to be ok

    // if we are still syncing and there was no known ping for this mn for quite a while
    // (NOTE: assuming that SHROUDNODE_EXPIRATION_SECONDS/2 should be enough to finish mn list sync)
    if (!shroudnodeSync.IsShroudnodeListSynced() && !pmn->IsPingedWithin(SHROUDNODE_EXPIRATION_SECONDS / 2)) {
        // let's bump sync timeout
        LogPrint("shroudnode", "CShroudnodePing::CheckAndUpdate -- bumping sync timeout, shroudnode=%s\n", vin.prevout.ToStringShort());
        shroudnodeSync.AddedShroudnodeList();
        GetMainSignals().UpdatedShroudnode(*pmn);
    }

    // let's store this ping as the last one
    LogPrint("shroudnode", "CShroudnodePing::CheckAndUpdate -- Shroudnode ping accepted, shroudnode=%s\n", vin.prevout.ToStringShort());
    pmn->SetLastPing(*this);

    // and update mnodeman.mapSeenShroudnodeBroadcast.lastPing which is probably outdated
    CShroudnodeBroadcast mnb(*pmn);
    uint256 hash = mnb.GetHash();
    if (mnodeman.mapSeenShroudnodeBroadcast.count(hash)) {
        mnodeman.mapSeenShroudnodeBroadcast[hash].second.SetLastPing(*this);
    }

    pmn->Check(true); // force update, ignoring cache
    if (!pmn->IsEnabled()) return false;

    LogPrint("shroudnode", "CShroudnodePing::CheckAndUpdate -- Shroudnode ping acceepted and relayed, shroudnode=%s\n", vin.prevout.ToStringShort());
    Relay();

    return true;
}

void CShroudnodePing::Relay() {
    CInv inv(MSG_SHROUDNODE_PING, GetHash());
    RelayInv(inv);
}

//void CShroudnode::AddGovernanceVote(uint256 nGovernanceObjectHash)
//{
//    if(mapGovernanceObjectsVotedOn.count(nGovernanceObjectHash)) {
//        mapGovernanceObjectsVotedOn[nGovernanceObjectHash]++;
//    } else {
//        mapGovernanceObjectsVotedOn.insert(std::make_pair(nGovernanceObjectHash, 1));
//    }
//}

//void CShroudnode::RemoveGovernanceObject(uint256 nGovernanceObjectHash)
//{
//    std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.find(nGovernanceObjectHash);
//    if(it == mapGovernanceObjectsVotedOn.end()) {
//        return;
//    }
//    mapGovernanceObjectsVotedOn.erase(it);
//}

void CShroudnode::UpdateWatchdogVoteTime() {
    LOCK(cs);
    nTimeLastWatchdogVote = GetTime();
}

/**
*   FLAG GOVERNANCE ITEMS AS DIRTY
*
*   - When shroudnode come and go on the network, we must flag the items they voted on to recalc it's cached flags
*
*/
//void CShroudnode::FlagGovernanceItemsAsDirty()
//{
//    std::vector<uint256> vecDirty;
//    {
//        std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.begin();
//        while(it != mapGovernanceObjectsVotedOn.end()) {
//            vecDirty.push_back(it->first);
//            ++it;
//        }
//    }
//    for(size_t i = 0; i < vecDirty.size(); ++i) {
//        mnodeman.AddDirtyGovernanceObjectHash(vecDirty[i]);
//    }
//}