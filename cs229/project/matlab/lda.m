function [docTopicProbs, topicWordProbs] = lda(M1, numTopics, numIters, alpha, beta)
  numDocs = size(M1, 1);
  vocabSize = size(M1, 2);

  docTopicCountMat = zeros(numDocs, numTopics);
  topicWordCountMat = zeros(numTopics, vocabSize);
  docWordTopicMat = zeros(numDocs, vocabSize);

  words = {};

  % Initialize each word in each document to a random topic
  for docIdx=1:numDocs
    if mod(docIdx, 100) == 0
        docIdx
    end
    for wordIdx=1:vocabSize
      numWords = M1(docIdx, wordIdx);
      for i=1:numWords
        topicIdx = randi(numTopics);
        words = [words, [docIdx, wordIdx, topicIdx]];
        docWordTopicMat(docIdx, wordIdx) = topicIdx;
        docTopicCountMat(docIdx, topicIdx) = docTopicCountMat(docIdx, topicIdx) + 1;
        topicWordCountMat(topicIdx, wordIdx) = topicWordCountMat(topicIdx, wordIdx) + 1;
      end
    end
  end

  length(words)

  for iter=1:numIters
    iter
    for i=1:length(words)
      docAndWordAndTopicIdx = words{i};
      docIdx = docAndWordAndTopicIdx(1);
      wordIdx = docAndWordAndTopicIdx(2);

      oldTopicIdx = docAndWordAndTopicIdx(3);
      newTopicIdx = gibbsSample(docIdx, wordIdx, alpha, beta, docTopicCountMat, topicWordCountMat);

      if oldTopicIdx ~= newTopicIdx
        words{i} = [docIdx, wordIdx, newTopicIdx];

        docWordTopicMat(docIdx, wordIdx) = newTopicIdx;

        docTopicCountMat(docIdx, newTopicIdx) = docTopicCountMat(docIdx, newTopicIdx) + 1;
        topicWordCountMat(newTopicIdx, wordIdx) = topicWordCountMat(newTopicIdx, wordIdx) + 1;

        docTopicCountMat(docIdx, oldTopicIdx) = docTopicCountMat(docIdx, oldTopicIdx) - 1;
        topicWordCountMat(oldTopicIdx, wordIdx) = topicWordCountMat(oldTopicIdx, wordIdx) - 1;
      end
    end
  end

  docTopicProbs = zeros(numDocs, numTopics);
  for docIdx=1:numDocs
    docTopicCounts = docTopicCountMat(docIdx, :);
    docTopicSum = sum(docTopicCountMat(docIdx, :));
    docTopicProbs(docIdx, :) = (docTopicCounts + alpha) ./ (docTopicSum + numTopics * alpha);
  end

  topicWordProbs = zeros(numTopics, vocabSize);
  for topicIdx=1:numTopics
    topicWordCounts = topicWordCountMat(topicIdx, :);
    topicWordSum = sum(topicWordCountMat(topicIdx, :));
    topicWordProbs(topicIdx, :) = (topicWordCounts + beta) ./ (topicWordSum + vocabSize * beta);
  end