/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define LOG_TAG "LatinIME: correction.cpp"

#include "correction.h"
#include "dictionary.h"
#include "proximity_info.h"

namespace latinime {

//////////////////////
// inline functions //
//////////////////////
static const char QUOTE = '\'';

inline bool Correction::isQuote(const unsigned short c) {
    const unsigned short userTypedChar = mProximityInfo->getPrimaryCharAt(mInputIndex);
    return (c == QUOTE && userTypedChar != QUOTE);
}

////////////////
// Correction //
////////////////

Correction::Correction(const int typedLetterMultiplier, const int fullWordMultiplier)
        : TYPED_LETTER_MULTIPLIER(typedLetterMultiplier), FULL_WORD_MULTIPLIER(fullWordMultiplier) {
}

void Correction::initCorrection(const ProximityInfo *pi, const int inputLength,
        const int maxDepth) {
    mProximityInfo = pi;
    mInputLength = inputLength;
    mMaxDepth = maxDepth;
    mMaxEditDistance = mInputLength < 5 ? 2 : mInputLength / 2;
}

void Correction::initCorrectionState(
        const int rootPos, const int childCount, const bool traverseAll) {
    latinime::initCorrectionState(mCorrectionStates, rootPos, childCount, traverseAll);
    // TODO: remove
    mCorrectionStates[0].mTransposedPos = mTransposedPos;
    mCorrectionStates[0].mExcessivePos = mExcessivePos;
    mCorrectionStates[0].mSkipPos = mSkipPos;
}

void Correction::setCorrectionParams(const int skipPos, const int excessivePos,
        const int transposedPos, const int spaceProximityPos, const int missingSpacePos) {
    // TODO: remove
    mTransposedPos = transposedPos;
    mExcessivePos = excessivePos;
    mSkipPos = skipPos;
    // TODO: remove
    mCorrectionStates[0].mTransposedPos = transposedPos;
    mCorrectionStates[0].mExcessivePos = excessivePos;
    mCorrectionStates[0].mSkipPos = skipPos;

    mSpaceProximityPos = spaceProximityPos;
    mMissingSpacePos = missingSpacePos;
}

void Correction::checkState() {
    if (DEBUG_DICT) {
        int inputCount = 0;
        if (mSkipPos >= 0) ++inputCount;
        if (mExcessivePos >= 0) ++inputCount;
        if (mTransposedPos >= 0) ++inputCount;
        // TODO: remove this assert
        assert(inputCount <= 1);
    }
}

int Correction::getFreqForSplitTwoWords(const int firstFreq, const int secondFreq) {
    return Correction::RankingAlgorithm::calcFreqForSplitTwoWords(firstFreq, secondFreq, this);
}

int Correction::getFinalFreq(const int freq, unsigned short **word, int *wordLength) {
    const int outputIndex = mTerminalOutputIndex;
    const int inputIndex = mTerminalInputIndex;
    *wordLength = outputIndex + 1;
    if (mProximityInfo->sameAsTyped(mWord, outputIndex + 1) || outputIndex < MIN_SUGGEST_DEPTH) {
        return -1;
    }

    *word = mWord;
    return Correction::RankingAlgorithm::calculateFinalFreq(
            inputIndex, outputIndex, freq, mEditDistanceTable, this);
}

bool Correction::initProcessState(const int outputIndex) {
    if (mCorrectionStates[outputIndex].mChildCount <= 0) {
        return false;
    }
    mOutputIndex = outputIndex;
    --(mCorrectionStates[outputIndex].mChildCount);
    mInputIndex = mCorrectionStates[outputIndex].mInputIndex;
    mNeedsToTraverseAllNodes = mCorrectionStates[outputIndex].mNeedsToTraverseAllNodes;

    mProximityCount = mCorrectionStates[outputIndex].mProximityCount;
    mTransposedCount = mCorrectionStates[outputIndex].mTransposedCount;
    mExcessiveCount = mCorrectionStates[outputIndex].mExcessiveCount;
    mSkippedCount = mCorrectionStates[outputIndex].mSkippedCount;
    mLastCharExceeded = mCorrectionStates[outputIndex].mLastCharExceeded;

    mTransposedPos = mCorrectionStates[outputIndex].mTransposedPos;
    mExcessivePos = mCorrectionStates[outputIndex].mExcessivePos;
    mSkipPos = mCorrectionStates[outputIndex].mSkipPos;

    mMatching = false;
    mProximityMatching = false;
    mTransposing = false;
    mExceeding = false;
    mSkipping = false;

    return true;
}

int Correction::goDownTree(
        const int parentIndex, const int childCount, const int firstChildPos) {
    mCorrectionStates[mOutputIndex].mParentIndex = parentIndex;
    mCorrectionStates[mOutputIndex].mChildCount = childCount;
    mCorrectionStates[mOutputIndex].mSiblingPos = firstChildPos;
    return mOutputIndex;
}

// TODO: remove
int Correction::getOutputIndex() {
    return mOutputIndex;
}

// TODO: remove
int Correction::getInputIndex() {
    return mInputIndex;
}

// TODO: remove
bool Correction::needsToTraverseAllNodes() {
    return mNeedsToTraverseAllNodes;
}

void Correction::incrementInputIndex() {
    ++mInputIndex;
}

void Correction::incrementOutputIndex() {
    ++mOutputIndex;
    mCorrectionStates[mOutputIndex].mParentIndex = mCorrectionStates[mOutputIndex - 1].mParentIndex;
    mCorrectionStates[mOutputIndex].mChildCount = mCorrectionStates[mOutputIndex - 1].mChildCount;
    mCorrectionStates[mOutputIndex].mSiblingPos = mCorrectionStates[mOutputIndex - 1].mSiblingPos;
    mCorrectionStates[mOutputIndex].mInputIndex = mInputIndex;
    mCorrectionStates[mOutputIndex].mNeedsToTraverseAllNodes = mNeedsToTraverseAllNodes;

    mCorrectionStates[mOutputIndex].mProximityCount = mProximityCount;
    mCorrectionStates[mOutputIndex].mTransposedCount = mTransposedCount;
    mCorrectionStates[mOutputIndex].mExcessiveCount = mExcessiveCount;
    mCorrectionStates[mOutputIndex].mSkippedCount = mSkippedCount;

    mCorrectionStates[mOutputIndex].mSkipPos = mSkipPos;
    mCorrectionStates[mOutputIndex].mTransposedPos = mTransposedPos;
    mCorrectionStates[mOutputIndex].mExcessivePos = mExcessivePos;

    mCorrectionStates[mOutputIndex].mLastCharExceeded = mLastCharExceeded;

    mCorrectionStates[mOutputIndex].mMatching = mMatching;
    mCorrectionStates[mOutputIndex].mProximityMatching = mProximityMatching;
    mCorrectionStates[mOutputIndex].mTransposing = mTransposing;
    mCorrectionStates[mOutputIndex].mExceeding = mExceeding;
    mCorrectionStates[mOutputIndex].mSkipping = mSkipping;
}

void Correction::startToTraverseAllNodes() {
    mNeedsToTraverseAllNodes = true;
}

bool Correction::needsToPrune() const {
    return (mOutputIndex - 1 >= (mTransposedPos >= 0 ? mInputLength - 1 : mMaxDepth)
            || mProximityCount > mMaxEditDistance);
}

Correction::CorrectionType Correction::processSkipChar(
        const int32_t c, const bool isTerminal) {
    mWord[mOutputIndex] = c;
    if (needsToTraverseAllNodes() && isTerminal) {
        mTerminalInputIndex = mInputIndex;
        mTerminalOutputIndex = mOutputIndex;
        incrementOutputIndex();
        return TRAVERSE_ALL_ON_TERMINAL;
    } else {
        incrementOutputIndex();
        return TRAVERSE_ALL_NOT_ON_TERMINAL;
    }
}

Correction::CorrectionType Correction::processCharAndCalcState(
        const int32_t c, const bool isTerminal) {
    CorrectionType currentStateType = NOT_ON_TERMINAL;

    if (mExcessivePos >= 0) {
        if (mExcessiveCount == 0 && mExcessivePos < mOutputIndex) {
            ++mExcessivePos;
        }
        if (mExcessivePos < mInputLength - 1) {
            mExceeding = mExcessivePos == mInputIndex;
        }
    }

    if (mSkipPos >= 0) {
        if (mSkippedCount == 0 && mSkipPos < mOutputIndex) {
            if (DEBUG_DICT) {
                assert(mSkipPos == mOutputIndex - 1);
            }
            ++mSkipPos;
        }
        mSkipping = mSkipPos == mOutputIndex;
    }

    if (mTransposedPos >= 0) {
        if (mTransposedCount == 0 && mTransposedPos < mOutputIndex) {
                ++mTransposedPos;
            }
        if (mTransposedPos < mInputLength - 1) {
            mTransposing = mInputIndex == mTransposedPos;
        }
    }

    if (mNeedsToTraverseAllNodes || isQuote(c)) {
        return processSkipChar(c, isTerminal);
    } else {
        bool secondTransposing = false;
        if (mTransposedCount % 2 == 1) {
            if (mProximityInfo->getMatchedProximityId(mInputIndex - 1, c, false)
                    == ProximityInfo::SAME_OR_ACCENTED_OR_CAPITALIZED_CHAR) {
                ++mTransposedCount;
                secondTransposing = true;
            } else if (mCorrectionStates[mOutputIndex].mExceeding) {
                --mTransposedCount;
                ++mExcessiveCount;
                incrementInputIndex();
            } else {
                --mTransposedCount;
                return UNRELATED;
            }
        }

        // TODO: sum counters
        const bool checkProximityChars =
                !(mSkippedCount > 0 || mExcessivePos >= 0 || mTransposedPos >= 0);
        // TODO: do not check if second transposing
        const int matchedProximityCharId = mProximityInfo->getMatchedProximityId(
                mInputIndex, c, checkProximityChars);

        if (!secondTransposing && ProximityInfo::UNRELATED_CHAR == matchedProximityCharId) {
            if (mInputIndex - 1 < mInputLength && (mExceeding || mTransposing)
                    && mProximityInfo->getMatchedProximityId(mInputIndex + 1, c, false)
                            == ProximityInfo::SAME_OR_ACCENTED_OR_CAPITALIZED_CHAR) {
                if (mTransposing) {
                    ++mTransposedCount;
                } else {
                    ++mExcessiveCount;
                    incrementInputIndex();
                }
            } else if (mSkipping && mProximityCount == 0) {
                // Skip this letter and continue deeper
                ++mSkippedCount;
                return processSkipChar(c, isTerminal);
            } else if (checkProximityChars
                    && mInputIndex > 0
                    && mCorrectionStates[mOutputIndex].mProximityMatching
                    && mCorrectionStates[mOutputIndex].mSkipping
                    && mProximityInfo->getMatchedProximityId(mInputIndex - 1, c, false)
                            == ProximityInfo::SAME_OR_ACCENTED_OR_CAPITALIZED_CHAR) {
                // Note: This logic tries saving cases like contrst --> contrast -- "a" is one of
                // proximity chars of "s", but it should rather be handled as a skipped char.
                ++mSkippedCount;
                --mProximityCount;
                return processSkipChar(c, isTerminal);
            } else {
                return UNRELATED;
            }
        } else if (secondTransposing
                || ProximityInfo::SAME_OR_ACCENTED_OR_CAPITALIZED_CHAR == matchedProximityCharId) {
            // If inputIndex is greater than mInputLength, that means there is no
            // proximity chars. So, we don't need to check proximity.
            mMatching = true;
        } else if (ProximityInfo::NEAR_PROXIMITY_CHAR == matchedProximityCharId) {
            mProximityMatching = true;
            incrementProximityCount();
        }

        mWord[mOutputIndex] = c;

        mLastCharExceeded = mExcessiveCount == 0 && mSkippedCount == 0
                && mProximityCount == 0 && mTransposedCount == 0
                // TODO: remove this line once excessive correction is conmibned to others.
                && mExcessivePos >= 0 && (mInputIndex == mInputLength - 2);
        const bool isSameAsUserTypedLength = (mInputLength == mInputIndex + 1) || mLastCharExceeded;
        if (mLastCharExceeded) {
            // TODO: Decrement mExcessiveCount if next char is matched word.
            ++mExcessiveCount;
        }
        if (isSameAsUserTypedLength && isTerminal) {
            mTerminalInputIndex = mInputIndex;
            mTerminalOutputIndex = mOutputIndex;
            currentStateType = ON_TERMINAL;
        }
        // Start traversing all nodes after the index exceeds the user typed length
        if (isSameAsUserTypedLength) {
            startToTraverseAllNodes();
        }

        // Finally, we are ready to go to the next character, the next "virtual node".
        // We should advance the input index.
        // We do this in this branch of the 'if traverseAllNodes' because we are still matching
        // characters to input; the other branch is not matching them but searching for
        // completions, this is why it does not have to do it.
        incrementInputIndex();
    }

    // Also, the next char is one "virtual node" depth more than this char.
    incrementOutputIndex();

    return currentStateType;
}

Correction::~Correction() {
}

/////////////////////////
// static inline utils //
/////////////////////////

static const int TWO_31ST_DIV_255 = S_INT_MAX / 255;
static inline int capped255MultForFullMatchAccentsOrCapitalizationDifference(const int num) {
    return (num < TWO_31ST_DIV_255 ? 255 * num : S_INT_MAX);
}

static const int TWO_31ST_DIV_2 = S_INT_MAX / 2;
inline static void multiplyIntCapped(const int multiplier, int *base) {
    const int temp = *base;
    if (temp != S_INT_MAX) {
        // Branch if multiplier == 2 for the optimization
        if (multiplier == 2) {
            *base = TWO_31ST_DIV_2 >= temp ? temp << 1 : S_INT_MAX;
        } else {
            const int tempRetval = temp * multiplier;
            *base = tempRetval >= temp ? tempRetval : S_INT_MAX;
        }
    }
}

inline static int powerIntCapped(const int base, const int n) {
    if (n == 0) return 1;
    if (base == 2) {
        return n < 31 ? 1 << n : S_INT_MAX;
    } else {
        int ret = base;
        for (int i = 1; i < n; ++i) multiplyIntCapped(base, &ret);
        return ret;
    }
}

inline static void multiplyRate(const int rate, int *freq) {
    if (*freq != S_INT_MAX) {
        if (*freq > 1000000) {
            *freq /= 100;
            multiplyIntCapped(rate, freq);
        } else {
            multiplyIntCapped(rate, freq);
            *freq /= 100;
        }
    }
}

inline static int getQuoteCount(const unsigned short* word, const int length) {
    int quoteCount = 0;
    for (int i = 0; i < length; ++i) {
        if(word[i] == '\'') {
            ++quoteCount;
        }
    }
    return quoteCount;
}

/* static */
inline static int editDistance(
        int* editDistanceTable, const unsigned short* input,
        const int inputLength, const unsigned short* output, const int outputLength) {
    // dp[li][lo] dp[a][b] = dp[ a * lo + b]
    int* dp = editDistanceTable;
    const int li = inputLength + 1;
    const int lo = outputLength + 1;
    for (int i = 0; i < li; ++i) {
        dp[lo * i] = i;
    }
    for (int i = 0; i < lo; ++i) {
        dp[i] = i;
    }

    for (int i = 0; i < li - 1; ++i) {
        for (int j = 0; j < lo - 1; ++j) {
            const uint32_t ci = Dictionary::toBaseLowerCase(input[i]);
            const uint32_t co = Dictionary::toBaseLowerCase(output[j]);
            const uint16_t cost = (ci == co) ? 0 : 1;
            dp[(i + 1) * lo + (j + 1)] = min(dp[i * lo + (j + 1)] + 1,
                    min(dp[(i + 1) * lo + j] + 1, dp[i * lo + j] + cost));
            if (li > 0 && lo > 0
                    && ci == Dictionary::toBaseLowerCase(output[j - 1])
                    && co == Dictionary::toBaseLowerCase(input[i - 1])) {
                dp[(i + 1) * lo + (j + 1)] = min(
                        dp[(i + 1) * lo + (j + 1)], dp[(i - 1) * lo + (j - 1)] + cost);
            }
        }
    }

    if (DEBUG_EDIT_DISTANCE) {
        LOGI("IN = %d, OUT = %d", inputLength, outputLength);
        for (int i = 0; i < li; ++i) {
            for (int j = 0; j < lo; ++j) {
                LOGI("EDIT[%d][%d], %d", i, j, dp[i * lo + j]);
            }
        }
    }
    return dp[li * lo - 1];
}

//////////////////////
// RankingAlgorithm //
//////////////////////

/* static */
int Correction::RankingAlgorithm::calculateFinalFreq(const int inputIndex, const int outputIndex,
        const int freq, int* editDistanceTable, const Correction* correction) {
    const int excessivePos = correction->getExcessivePos();
    const int transposedPos = correction->getTransposedPos();
    const int inputLength = correction->mInputLength;
    const int typedLetterMultiplier = correction->TYPED_LETTER_MULTIPLIER;
    const int fullWordMultiplier = correction->FULL_WORD_MULTIPLIER;
    const ProximityInfo *proximityInfo = correction->mProximityInfo;
    const int skippedCount = correction->mSkippedCount;
    const int transposedCount = correction->mTransposedCount;
    const int excessiveCount = correction->mExcessiveCount;
    const int proximityMatchedCount = correction->mProximityCount;
    const bool lastCharExceeded = correction->mLastCharExceeded;
    if (skippedCount >= inputLength || inputLength == 0) {
        return -1;
    }

    // TODO: remove
    if (transposedPos >= 0 && transposedCount == 0) {
        return -1;
    }

    // TODO: remove
    if (excessivePos >= 0 && excessiveCount == 0) {
        return -1;
    }

    const bool sameLength = lastCharExceeded ? (inputLength == inputIndex + 2)
            : (inputLength == inputIndex + 1);

    // TODO: use mExcessiveCount
    int matchCount = inputLength - correction->mProximityCount - (excessivePos >= 0 ? 1 : 0);

    const unsigned short* word = correction->mWord;
    const bool skipped = skippedCount > 0;

    const int quoteDiffCount = max(0, getQuoteCount(word, outputIndex + 1)
            - getQuoteCount(proximityInfo->getPrimaryInputWord(), inputLength));

    // TODO: Calculate edit distance for transposed and excessive
    int matchWeight;
    int ed = 0;
    int adJustedProximityMatchedCount = proximityMatchedCount;

    // TODO: Optimize this.
    if (excessivePos < 0 && transposedPos < 0 && (proximityMatchedCount > 0 || skipped)) {
        const unsigned short* primaryInputWord = proximityInfo->getPrimaryInputWord();
        ed = editDistance(editDistanceTable, primaryInputWord,
                inputLength, word, outputIndex + 1);
        matchWeight = powerIntCapped(typedLetterMultiplier, outputIndex + 1 - ed);
        if (ed == 1 && inputLength == outputIndex) {
            // Promote a word with just one skipped char
            multiplyRate(WORDS_WITH_JUST_ONE_CORRECTION_PROMOTION_RATE, &matchWeight);
        }
        ed = max(0, ed - quoteDiffCount);
        adJustedProximityMatchedCount = min(max(0, ed - (outputIndex + 1 - inputLength)),
                proximityMatchedCount);
    } else {
        matchWeight = powerIntCapped(typedLetterMultiplier, matchCount);
    }

    // TODO: Demote by edit distance
    int finalFreq = freq * matchWeight;

    ///////////////////////////////////////////////
    // Promotion and Demotion for each correction

    // Demotion for a word with missing character
    if (skipped) {
        const int demotionRate = WORDS_WITH_MISSING_CHARACTER_DEMOTION_RATE
                * (10 * inputLength - WORDS_WITH_MISSING_CHARACTER_DEMOTION_START_POS_10X)
                / (10 * inputLength
                        - WORDS_WITH_MISSING_CHARACTER_DEMOTION_START_POS_10X + 10);
        if (DEBUG_DICT_FULL) {
            LOGI("Demotion rate for missing character is %d.", demotionRate);
        }
        multiplyRate(demotionRate, &finalFreq);
    }

    // Demotion for a word with transposed character
    if (transposedPos >= 0) multiplyRate(
            WORDS_WITH_TRANSPOSED_CHARACTERS_DEMOTION_RATE, &finalFreq);

    // Demotion for a word with excessive character
    if (excessivePos >= 0) {
        multiplyRate(WORDS_WITH_EXCESSIVE_CHARACTER_DEMOTION_RATE, &finalFreq);
        if (!proximityInfo->existsAdjacentProximityChars(inputIndex)) {
            // If an excessive character is not adjacent to the left char or the right char,
            // we will demote this word.
            multiplyRate(WORDS_WITH_EXCESSIVE_CHARACTER_OUT_OF_PROXIMITY_DEMOTION_RATE, &finalFreq);
        }
    }

    // Promotion for a word with proximity characters
    for (int i = 0; i < adJustedProximityMatchedCount; ++i) {
        // A word with proximity corrections
        if (DEBUG_DICT_FULL) {
            LOGI("Found a proximity correction.");
        }
        multiplyIntCapped(typedLetterMultiplier, &finalFreq);
        multiplyRate(WORDS_WITH_PROXIMITY_CHARACTER_DEMOTION_RATE, &finalFreq);
    }

    const int errorCount = proximityMatchedCount + skippedCount;
    multiplyRate(
            100 - CORRECTION_COUNT_RATE_DEMOTION_RATE_BASE * errorCount / inputLength, &finalFreq);

    // Promotion for an exactly matched word
    if (matchCount == outputIndex + 1) {
        // Full exact match
        if (sameLength && transposedPos < 0 && !skipped && excessivePos < 0) {
            finalFreq = capped255MultForFullMatchAccentsOrCapitalizationDifference(finalFreq);
        }
    }

    // Promote a word with no correction
    if (proximityMatchedCount == 0 && transposedPos < 0 && !skipped && excessivePos < 0) {
        multiplyRate(FULL_MATCHED_WORDS_PROMOTION_RATE, &finalFreq);
    }

    // TODO: Check excessive count and transposed count
    // TODO: Remove this if possible
    /*
         If the last character of the user input word is the same as the next character
         of the output word, and also all of characters of the user input are matched
         to the output word, we'll promote that word a bit because
         that word can be considered the combination of skipped and matched characters.
         This means that the 'sm' pattern wins over the 'ma' pattern.
         e.g.)
         shel -> shell [mmmma] or [mmmsm]
         hel -> hello [mmmaa] or [mmsma]
         m ... matching
         s ... skipping
         a ... traversing all
     */
    if (matchCount == inputLength && matchCount >= 2 && !skipped
            && word[matchCount] == word[matchCount - 1]) {
        multiplyRate(WORDS_WITH_MATCH_SKIP_PROMOTION_RATE, &finalFreq);
    }

    if (sameLength) {
        multiplyIntCapped(fullWordMultiplier, &finalFreq);
    }

    if (DEBUG_DICT_FULL) {
        LOGI("calc: %d, %d", outputIndex, sameLength);
    }

    return finalFreq;
}

/* static */
int Correction::RankingAlgorithm::calcFreqForSplitTwoWords(
        const int firstFreq, const int secondFreq, const Correction* correction) {
    const int spaceProximityPos = correction->mSpaceProximityPos;
    const int missingSpacePos = correction->mMissingSpacePos;
    if (DEBUG_DICT) {
        int inputCount = 0;
        if (spaceProximityPos >= 0) ++inputCount;
        if (missingSpacePos >= 0) ++inputCount;
        assert(inputCount <= 1);
    }
    const bool isSpaceProximity = spaceProximityPos >= 0;
    const int inputLength = correction->mInputLength;
    const int firstWordLength = isSpaceProximity ? spaceProximityPos : missingSpacePos;
    const int secondWordLength = isSpaceProximity
            ? (inputLength - spaceProximityPos - 1)
            : (inputLength - missingSpacePos);
    const int typedLetterMultiplier = correction->TYPED_LETTER_MULTIPLIER;

    if (firstWordLength == 0 || secondWordLength == 0) {
        return 0;
    }
    const int firstDemotionRate = 100 - 100 / (firstWordLength + 1);
    int tempFirstFreq = firstFreq;
    multiplyRate(firstDemotionRate, &tempFirstFreq);

    const int secondDemotionRate = 100 - 100 / (secondWordLength + 1);
    int tempSecondFreq = secondFreq;
    multiplyRate(secondDemotionRate, &tempSecondFreq);

    const int totalLength = firstWordLength + secondWordLength;

    // Promote pairFreq with multiplying by 2, because the word length is the same as the typed
    // length.
    int totalFreq = tempFirstFreq + tempSecondFreq;

    // This is a workaround to try offsetting the not-enough-demotion which will be done in
    // calcNormalizedScore in Utils.java.
    // In calcNormalizedScore the score will be demoted by (1 - 1 / length)
    // but we demoted only (1 - 1 / (length + 1)) so we will additionally adjust freq by
    // (1 - 1 / length) / (1 - 1 / (length + 1)) = (1 - 1 / (length * length))
    const int normalizedScoreNotEnoughDemotionAdjustment = 100 - 100 / (totalLength * totalLength);
    multiplyRate(normalizedScoreNotEnoughDemotionAdjustment, &totalFreq);

    // At this moment, totalFreq is calculated by the following formula:
    // (firstFreq * (1 - 1 / (firstWordLength + 1)) + secondFreq * (1 - 1 / (secondWordLength + 1)))
    //        * (1 - 1 / totalLength) / (1 - 1 / (totalLength + 1))

    multiplyIntCapped(powerIntCapped(typedLetterMultiplier, totalLength), &totalFreq);

    // This is another workaround to offset the demotion which will be done in
    // calcNormalizedScore in Utils.java.
    // In calcNormalizedScore the score will be demoted by (1 - 1 / length) so we have to promote
    // the same amount because we already have adjusted the synthetic freq of this "missing or
    // mistyped space" suggestion candidate above in this method.
    const int normalizedScoreDemotionRateOffset = (100 + 100 / totalLength);
    multiplyRate(normalizedScoreDemotionRateOffset, &totalFreq);

    if (isSpaceProximity) {
        // A word pair with one space proximity correction
        if (DEBUG_DICT) {
            LOGI("Found a word pair with space proximity correction.");
        }
        multiplyIntCapped(typedLetterMultiplier, &totalFreq);
        multiplyRate(WORDS_WITH_PROXIMITY_CHARACTER_DEMOTION_RATE, &totalFreq);
    }

    multiplyRate(WORDS_WITH_MISSING_SPACE_CHARACTER_DEMOTION_RATE, &totalFreq);
    return totalFreq;
}

} // namespace latinime
