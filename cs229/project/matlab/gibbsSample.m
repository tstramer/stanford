function [sample] = gibbsSample(docIdx, wordIdx, alpha, beta, docTopicCountMat, topicWordCountMat)
  numWords = size(topicWordCountMat, 2);

  docTopicCounts = docTopicCountMat(docIdx, :);
  topicWordCounts = topicWordCountMat(:, wordIdx)';
  topicAllWordsCounts = sum(topicWordCountMat, 2)';

  params = (docTopicCounts + alpha) .* (topicWordCounts + beta) ./...
   (topicAllWordsCounts + numWords * beta);

  probs = params / sum(params);

  sample = find(mnrnd(1, probs) == 1);
